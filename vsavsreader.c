/*
  vsavsreader.c: AviSynth Script Reader for VapourSynth

  Copyright (C) 2012  Oka Motofumi

  Author: Oka Motofumi (chikuzen.mo at gmail dot com)

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with Libav; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/


#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>

#define AVSC_NO_DECLSPEC
#undef EXTERN_C
#include "avisynth_c.h"
#define AVS_INTERFACE_25 2

#include "VapourSynth.h"


#define AVSC_DECLARE_FUNC(name) name##_func name
typedef struct avsr_handle {
    VSVideoInfo vs_vi[2];
    int avs_version;
    int bitdepth;
    AVS_Clip *clip;
    AVS_Clip *rgbaclip[4];
    int three_or_four;
    int (__stdcall *write_frame)(struct avsr_handle *, int, VSFrameRef *,
                                 const VSAPI *, VSCore *);
    AVS_ScriptEnvironment *env;
    const AVS_VideoInfo *avs_vi;
    HMODULE library;
    struct {
        AVSC_DECLARE_FUNC(avs_clip_get_error);
        AVSC_DECLARE_FUNC(avs_create_script_environment);
        AVSC_DECLARE_FUNC(avs_delete_script_environment);
        AVSC_DECLARE_FUNC(avs_get_error);
        AVSC_DECLARE_FUNC(avs_get_frame);
        AVSC_DECLARE_FUNC(avs_get_version);
        AVSC_DECLARE_FUNC(avs_get_video_info);
        AVSC_DECLARE_FUNC(avs_function_exists);
        AVSC_DECLARE_FUNC(avs_invoke);
        AVSC_DECLARE_FUNC(avs_bit_blt);
        AVSC_DECLARE_FUNC(avs_release_clip);
        AVSC_DECLARE_FUNC(avs_release_value);
        AVSC_DECLARE_FUNC(avs_release_video_frame);
        AVSC_DECLARE_FUNC(avs_take_clip);
    } func;
} avsr_hnd_t;
#undef AVSC_DECLARE_FUNC


#define LOAD_AVS_FUNC(name, continue_on_fail)\
{\
    ah->func.name = (name##_func)GetProcAddress(ah->library, #name);\
    if (!continue_on_fail && !ah->func.name)\
        goto fail;\
}

static int __stdcall load_avisynth_dll(avsr_hnd_t *ah)
{
    ah->library = LoadLibrary("avisynth");
    if(!ah->library) {
        return -1;
    }

    LOAD_AVS_FUNC(avs_clip_get_error, 0);
    LOAD_AVS_FUNC(avs_create_script_environment, 0);
    LOAD_AVS_FUNC(avs_delete_script_environment, 1);
    LOAD_AVS_FUNC(avs_get_error, 1);
    LOAD_AVS_FUNC(avs_get_frame, 0);
    LOAD_AVS_FUNC(avs_get_version, 0);
    LOAD_AVS_FUNC(avs_get_video_info, 0);
    LOAD_AVS_FUNC(avs_function_exists, 0);
    LOAD_AVS_FUNC(avs_invoke, 0);
    LOAD_AVS_FUNC(avs_bit_blt, 0);
    LOAD_AVS_FUNC(avs_release_clip, 0);
    LOAD_AVS_FUNC(avs_release_value, 0);
    LOAD_AVS_FUNC(avs_release_video_frame, 0);
    LOAD_AVS_FUNC(avs_take_clip, 0);

    return 0;

fail:
    FreeLibrary(ah->library);
    return -1;
}
#undef LOAD_AVS_FUNC


static int __stdcall get_avisynth_version(avsr_hnd_t *ah)
{
    if (!ah->func.avs_function_exists(ah->env, "VersionNumber")) {
        return 0;
    }

    AVS_Value ver = ah->func.avs_invoke(ah->env, "VersionNumber",
                                        avs_new_value_array(NULL, 0), NULL);
    if (avs_is_error(ver) || !avs_is_float(ver)) {
        return 0;
    }

    int version = (int)(avs_as_float(ver) * 100 + 0.5);
    ah->func.avs_release_value(ver);

    return version;
}


static AVS_Value __stdcall
invoke_avs_filter(avsr_hnd_t *ah, AVS_Value before, const char *filter)
{
    ah->func.avs_release_clip(ah->clip);
    AVS_Value after = ah->func.avs_invoke(ah->env, filter, before, NULL);
    ah->func.avs_release_value(before);
    ah->clip = ah->func.avs_take_clip(after, ah->env);
    ah->avs_vi = ah->func.avs_get_video_info(ah->clip);
    return after;
}


static void __stdcall take_y8_clips_from_packed_rgb(avsr_hnd_t *ah, AVS_Value res)
{
    const char *filter[] = {"ShowRed", "ShowGreen", "ShowBlue", "ShowAlpha"};
    AVS_Value args[] = {res, avs_new_value_string("Y8")};
    AVS_Value array = avs_new_value_array(args, 2);

    for (int i = 0, num = ah->three_or_four; i < num; i++) {
        AVS_Value tmp = ah->func.avs_invoke(ah->env, filter[i], array, NULL);
        ah->rgbaclip[i] = ah->func.avs_take_clip(tmp, ah->env);
        ah->func.avs_release_value(tmp);
    }
}


#define RET_IF_ERROR(cond, ...) {\
    if (cond) {\
        snprintf(msg, 240, __VA_ARGS__);\
        return avs_void;\
    }\
}\

#define INVALID_IF_ERROR(cond, ...) {\
    if (cond) {\
        snprintf(msg, 240, __VA_ARGS__);\
        goto invalid;\
    }\
}

static AVS_Value __stdcall
initialize_avisynth(avsr_hnd_t *ah, const char *input, const char *mode,
                    int alpha, char *msg)
{
    RET_IF_ERROR(load_avisynth_dll(ah), "failed to load avisynth.dll");

    ah->env = ah->func.avs_create_script_environment(AVS_INTERFACE_25);
    RET_IF_ERROR(ah->func.avs_get_error && ah->func.avs_get_error(ah->env),
                 "avisynth environment has some trouble");

    RET_IF_ERROR(get_avisynth_version(ah) < 260,
                 "unsupported version of avisynth.dll was found.\n");

    AVS_Value res = ah->func.avs_invoke(ah->env, mode,
                                        avs_new_value_string(input), NULL);
    INVALID_IF_ERROR(avs_is_error(res) || !avs_defined(res),
                     "failed to invoke %s", input);

#ifdef BLAME_THE_FLUFF
    AVS_Value mt_test = ah->func.avs_invoke(ah->env, "GetMTMode",
                                            avs_new_value_bool(0), NULL);
    int mt_mode = avs_is_int(mt_test) ? avs_as_int(mt_test) : 0;
    ah->func.avs_release_value(mt_test);
    if (mt_mode > 0 && mt_mode < 5) {
        AVS_Value temp = ah->func.avs_invoke(ah->env, "Distributor", res, NULL);
        ah->func.avs_release_value(res);
        res = temp;
    }
#endif

    ah->clip = ah->func.avs_take_clip(res, ah->env);
    const char *err = ah->func.avs_clip_get_error(ah->clip);
    INVALID_IF_ERROR(err, "%s", err);

    ah->avs_vi = ah->func.avs_get_video_info(ah->clip);

    INVALID_IF_ERROR(!avs_has_video(ah->avs_vi), "clip has no video");

    ah->three_or_four = avs_is_rgb32(ah->avs_vi) && alpha ? 4 : 3;

    if (avs_is_yuy2(ah->avs_vi)) {
        res = invoke_avs_filter(ah, res, "ConvertToYV16");
    }

    if (avs_is_rgb(ah->avs_vi)) {
        take_y8_clips_from_packed_rgb(ah, res);
    }

    if (ah->bitdepth == 8) {
        return res;
    }

    INVALID_IF_ERROR(!avs_is_planar(ah->avs_vi) || avs_is_yv411(ah->avs_vi) ||
                     (avs_is_y8(ah->avs_vi) && ah->bitdepth != 16) ||
                     ((avs_is_yv24(ah->avs_vi) || avs_is_y8(ah->avs_vi)) && (ah->avs_vi->width & 1)) ||
                     ((avs_is_yv16(ah->avs_vi) || avs_is_yv12(ah->avs_vi)) && (ah->avs_vi->width & 3)),
                     "invalid bitdepth or resolution");

    return res;

invalid:
    ah->func.avs_release_value(res);
    return avs_void;
}


#define PIX_OR_DEPTH(pix, depth) (((uint64_t)pix << 8) | (depth))

static const VSFormat * __stdcall
get_vs_format(avsr_hnd_t *ah, VSCore *core, const VSAPI *vsapi, char *msg)
{
    uint64_t val = PIX_OR_DEPTH(ah->avs_vi->pixel_type, ah->bitdepth);
    struct {
        uint64_t avs_pix_type;
        int id;
    } table[] = {
        { PIX_OR_DEPTH(AVS_CS_BGR32, 8), pfRGB24     },
        { PIX_OR_DEPTH(AVS_CS_BGR24, 8), pfRGB24     },
        { PIX_OR_DEPTH(AVS_CS_YV24,  8), pfYUV444P8  },
        { PIX_OR_DEPTH(AVS_CS_YV24,  9), pfYUV444P9  },
        { PIX_OR_DEPTH(AVS_CS_YV24, 10), pfYUV444P10 },
        { PIX_OR_DEPTH(AVS_CS_YV24, 16), pfYUV444P16 },
        { PIX_OR_DEPTH(AVS_CS_YV16,  8), pfYUV422P8  },
        { PIX_OR_DEPTH(AVS_CS_YV16,  9), pfYUV422P9  },
        { PIX_OR_DEPTH(AVS_CS_YV16, 10), pfYUV422P10 },
        { PIX_OR_DEPTH(AVS_CS_YV16, 16), pfYUV422P16 },
        { PIX_OR_DEPTH(AVS_CS_YV411, 8), pfYUV411P8  },
        { PIX_OR_DEPTH(AVS_CS_I420,  8), pfYUV420P8  },
        { PIX_OR_DEPTH(AVS_CS_I420,  9), pfYUV420P9  },
        { PIX_OR_DEPTH(AVS_CS_I420, 10), pfYUV420P10 },
        { PIX_OR_DEPTH(AVS_CS_I420, 16), pfYUV420P16 },
        { PIX_OR_DEPTH(AVS_CS_YV12,  8), pfYUV420P8  },
        { PIX_OR_DEPTH(AVS_CS_YV12,  9), pfYUV420P9  },
        { PIX_OR_DEPTH(AVS_CS_YV12, 10), pfYUV420P10 },
        { PIX_OR_DEPTH(AVS_CS_YV12, 16), pfYUV420P16 },
        { PIX_OR_DEPTH(AVS_CS_Y8,    8), pfGray8     },
        { PIX_OR_DEPTH(AVS_CS_Y8,   16), pfGray16    },
        { val, 0 }
    };

    int i;
    for (i = 0; table[i].avs_pix_type != val; i++);
    if (table[i].id == 0) {
        sprintf(msg, "couldn't found valid format type");
        return NULL;
    }

    return vsapi->getFormatPreset(table[i].id, core);

}
#undef PIX_OR_DEPTH


static int __stdcall
set_vs_videoinfo(avsr_hnd_t *ah, VSCore *core, const VSAPI *vsapi, char *msg)
{
    ah->vs_vi[0].format = get_vs_format(ah, core, vsapi, msg);
    if (!ah->vs_vi[0].format) {
        return -1;
    }

    ah->vs_vi[0].fpsNum = ah->avs_vi->fps_numerator;
    ah->vs_vi[0].fpsDen = ah->avs_vi->fps_denominator;
    ah->vs_vi[0].width = ah->bitdepth > 8 ? ah->avs_vi->width >> 1 :
                                         ah->avs_vi->width;
    ah->vs_vi[0].height = ah->avs_vi->height;
    ah->vs_vi[0].numFrames = ah->avs_vi->num_frames;

    if (ah->three_or_four == 4) {
        ah->vs_vi[1] = ah->vs_vi[0];
        ah->vs_vi[1].format = vsapi->getFormatPreset(pfGray8, core);
    }

    return 0;
}


static void __stdcall close_avisynth_dll(avsr_hnd_t *ah)
{
    if (ah->clip) {
        ah->func.avs_release_clip(ah->clip);
        ah->clip = NULL;
    }
    for (int i = 0; i < ah->three_or_four; i++) {
        if (ah->rgbaclip[i]) {
            ah->func.avs_release_clip(ah->rgbaclip[i]);
            ah->rgbaclip[i] = NULL;
        }
    }
    if (ah->func.avs_delete_script_environment) {
        ah->func.avs_delete_script_environment(ah->env);
    }

    FreeLibrary(ah->library);
}


static void __stdcall close_handler(avsr_hnd_t *ah)
{
    if (!ah) {
        return;
    }
    if (ah->library) {
        close_avisynth_dll(ah);
    }
    free(ah);
    ah = NULL;
}


static avsr_hnd_t * __stdcall
init_handler(const char *input, int bitdepth, int alpha, const char *mode,
             VSCore *core, const VSAPI *vsapi, char *msg)
{
    avsr_hnd_t *ah = (avsr_hnd_t *)calloc(sizeof(avsr_hnd_t), 1);
    if (!ah) {
        sprintf(msg, "failed to allocate handler");
        return NULL;
    }

    ah->bitdepth = bitdepth;
    AVS_Value res = initialize_avisynth(ah, input, mode, alpha, msg);
    if (!avs_is_clip(res)) {
        close_handler(ah);
        return NULL;
    }

    ah->func.avs_release_value(res);

    if (set_vs_videoinfo(ah, core, vsapi, msg)) {
        close_handler(ah);
        return NULL;
    }

    return ah;
}


static void __stdcall
vs_close(void *instance_data, VSCore *core, const VSAPI *vsapi)
{
    avsr_hnd_t *ah = (avsr_hnd_t *)instance_data;
    close_handler(ah);
}


static void __stdcall
vs_init(VSMap *in, VSMap *out, void **instance_data, VSNode *node,
        VSCore *core, const VSAPI *vsapi)
{
    avsr_hnd_t *ah = (avsr_hnd_t *)*instance_data;
    vsapi->setVideoInfo(ah->vs_vi, ah->three_or_four - 2, node);
}


static int __stdcall
write_frame_rgb(avsr_hnd_t *ah, int n, VSFrameRef *dst, const VSAPI *vsapi,
                VSCore *core)
{
    AVS_VideoFrame *src[3];
    for (int i = 0; i < 3; i++) {
        src[i] = ah->func.avs_get_frame(ah->rgbaclip[i], n);
        if (ah->func.avs_clip_get_error(ah->rgbaclip[i])) {
            ah->func.avs_release_video_frame(src[i]);
            return -1;
        }
        ah->func.avs_bit_blt(ah->env,
                             vsapi->getWritePtr(dst, i),
                             vsapi->getStride(dst, i),
                             avs_get_read_ptr(src[i]),
                             avs_get_pitch(src[i]),
                             avs_get_row_size(src[i]),
                             avs_get_height(src[i]));

        ah->func.avs_release_video_frame(src[i]);
    }

    return 0;
}


static int __stdcall
write_frame_yuv(avsr_hnd_t *ah, int n, VSFrameRef *dst, const VSAPI *vsapi,
                VSCore *core)
{
    AVS_VideoFrame *src = ah->func.avs_get_frame(ah->clip, n);
    if (ah->func.avs_clip_get_error(ah->clip)) {
        return -1;
    }

    const int plane[] = {AVS_PLANAR_Y, AVS_PLANAR_U, AVS_PLANAR_V};
    for (int i = 0, time = ah->vs_vi[0].format->numPlanes; i < time; i++) {
        ah->func.avs_bit_blt(ah->env,
                             vsapi->getWritePtr(dst, i),
                             vsapi->getStride(dst, i),
                             avs_get_read_ptr_p(src, plane[i]),
                             avs_get_pitch_p(src, plane[i]),
                             avs_get_row_size_p(src, plane[i]),
                             avs_get_height_p(src, plane[i]));
    }

    ah->func.avs_release_video_frame(src);

    return 0;
}


static VSFrameRef * __stdcall
get_alpha(int n, avsr_hnd_t *ah, VSCore *core, const VSAPI *vsapi)
{
    AVS_VideoFrame *src = ah->func.avs_get_frame(ah->rgbaclip[3], n);
    if (ah->func.avs_clip_get_error(ah->rgbaclip[3])) {
        ah->func.avs_release_video_frame(src);
            return NULL;
    }

    VSFrameRef *alpha = vsapi->newVideoFrame(ah->vs_vi[1].format,
                                             ah->vs_vi[1].width,
                                             ah->vs_vi[1].height,
                                             NULL, core);
    VSMap *props = vsapi->getFramePropsRW(alpha);
    vsapi->propSetInt(props, "_DurationNum", ah->avs_vi->fps_denominator, paReplace);
    vsapi->propSetInt(props, "_DurationDen", ah->avs_vi->fps_numerator, paReplace);

    ah->func.avs_bit_blt(ah->env,
                         vsapi->getWritePtr(alpha, 0),
                         vsapi->getStride(alpha, 0),
                         avs_get_read_ptr(src),
                         avs_get_pitch(src),
                         avs_get_row_size(src),
                         avs_get_height(src));

    ah->func.avs_release_video_frame(src);

    return alpha;
}


static const VSFrameRef * __stdcall
avsr_get_frame(int n, int activation_reason, void **instance_data,
               void **frame_data, VSFrameContext *frame_ctx, VSCore *core,
               const VSAPI *vsapi)
{
    if (activation_reason != arInitial) {
        return NULL;
    }

    avsr_hnd_t *ah = (avsr_hnd_t *)*instance_data;

    int frame_number = n;
    if (n >= ah->avs_vi->num_frames) {
        frame_number = ah->avs_vi->num_frames - 1;
    }

    VSFrameRef *dst = vsapi->newVideoFrame(ah->vs_vi[0].format, ah->vs_vi[0].width,
                                           ah->vs_vi[0].height, NULL, core);

    VSMap *props = vsapi->getFramePropsRW(dst);
    vsapi->propSetInt(props, "_DurationNum", ah->avs_vi->fps_denominator, paReplace);
    vsapi->propSetInt(props, "_DurationDen", ah->avs_vi->fps_numerator, paReplace);

    if (ah->write_frame(ah, frame_number, dst, vsapi, core)) {
        vsapi->setFilterError("failed to get frame from avisynth.dll",
                              frame_ctx);
        return NULL;
    }

    if (ah->three_or_four < 4) {
        return dst;
    }

    VSFrameRef *alpha = get_alpha(n, ah, core, vsapi);
    if (!alpha) {
        vsapi->setFilterError("failed to get alpha frame from avisynth.dll",
                              frame_ctx);
        return NULL;
    }

    return vsapi->getOutputIndex(frame_ctx) == 0 ? vsapi->cloneFrameRef(dst) :
                                                   vsapi->cloneFrameRef(alpha);
}


static void __stdcall
create_source(const VSMap *in, VSMap *out, void *user_data, VSCore *core,
              const VSAPI *vsapi)
{
    int err;
    const char *mode = (char *)user_data;
    char msg_buff[256] = {0};
    sprintf(msg_buff, "%s: ", mode);
    char *msg = msg_buff + strlen(msg_buff);

    int bitdepth = vsapi->propGetInt(in, "bitdepth", 0, &err);
    if (err) {
        bitdepth = 8;
    }
    if (bitdepth != 8 && bitdepth != 9 && bitdepth != 10 && bitdepth != 16) {
        sprintf(msg, "invalid bitdepth was specified");
        vsapi->setError(out, msg_buff);
        return;
    }

    const char *input =
        vsapi->propGetData(in, mode[0] == 'E' ? "lines" : "script", 0, 0);

    int alpha = (int)vsapi->propGetInt(in, "alpha", 0, &err);
    if (err || alpha != 0) {
        alpha = 1;
    }

    avsr_hnd_t *ah = init_handler(input, bitdepth, alpha, mode, core, vsapi, msg);
    if (!ah) {
        vsapi->setError(out, msg_buff);
        return;
    }

    ah->write_frame =
        avs_is_rgb(ah->avs_vi) ? write_frame_rgb : write_frame_yuv;

    vsapi->createFilter(in, out, mode, vs_init, avsr_get_frame, vs_close,
                        fmSerial, 0, ah, core);
}


__declspec(dllexport) void __stdcall VapourSynthPluginInit(
    VSConfigPlugin f_config, VSRegisterFunction f_register, VSPlugin *plugin)
{
    f_config("chikuzen.does.not.have.his.own.domain.avsr", "avsr",
             "AviSynth Script Reader for VapourSynth", VAPOURSYNTH_API_VERSION,
             1, plugin);
    f_register("Import", "script:data;bitdepth:int:opt;alpha:int:opt;",
               create_source, (void *)"Import", plugin);
    f_register("Eval", "lines:data;bitdepth:int:opt;alpha:int:opt;",
               create_source, (void *)"Eval", plugin);
}
