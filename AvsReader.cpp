/*
AvsReader.cpp

This file is a part of VS_AvsReader

Copyright (C) 2016  Oka Motofumi

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


#include <cstdint>
#include <algorithm>
#include <vector>
#include "AvsReader.h"
#include "myvshelper.h"


const AVS_Linkage* AVS_linkage = nullptr;


static void convert_utf8_to_ansi(const char* utf8, std::vector<char>& ansi)
{
    int length = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    std::vector<wchar_t> wchar;
    wchar.reserve(length);
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wchar.data(), length);

    length = WideCharToMultiByte(CP_THREAD_ACP, 0, wchar.data(), -1, nullptr, 0, 0, 0);
    ansi.reserve(length);
    WideCharToMultiByte(CP_THREAD_ACP, 0, wchar.data(), -1, ansi.data(), length, 0, 0);
}


static const VSFormat*
get_vs_format(int pixel_type, int bitdepth, VSCore *core, const VSAPI *api)
{
    auto pix_or_depth = [](uint64_t pix, uint64_t depth) {
        return (pix << 8) | depth;
    };

    uint64_t val = pix_or_depth(pixel_type, bitdepth);

    const struct {
        uint64_t avs_pix_type;
        int id;
    } table[] = {
        { pix_or_depth(VideoInfo::CS_BGR32, 8), pfRGB24     },
        { pix_or_depth(VideoInfo::CS_BGR24, 8), pfRGB24     },
        { pix_or_depth(VideoInfo::CS_YV24,  8), pfYUV444P8  },
        { pix_or_depth(VideoInfo::CS_YV24,  9), pfYUV444P9  },
        { pix_or_depth(VideoInfo::CS_YV24, 10), pfYUV444P10 },
        { pix_or_depth(VideoInfo::CS_YV24, 16), pfYUV444P16 },
        { pix_or_depth(VideoInfo::CS_YV16,  8), pfYUV422P8  },
        { pix_or_depth(VideoInfo::CS_YV16,  9), pfYUV422P9  },
        { pix_or_depth(VideoInfo::CS_YV16, 10), pfYUV422P10 },
        { pix_or_depth(VideoInfo::CS_YV16, 16), pfYUV422P16 },
        { pix_or_depth(VideoInfo::CS_YV411, 8), pfYUV411P8  },
        { pix_or_depth(VideoInfo::CS_I420,  8), pfYUV420P8  },
        { pix_or_depth(VideoInfo::CS_I420,  9), pfYUV420P9  },
        { pix_or_depth(VideoInfo::CS_I420, 10), pfYUV420P10 },
        { pix_or_depth(VideoInfo::CS_I420, 16), pfYUV420P16 },
        { pix_or_depth(VideoInfo::CS_YV12,  8), pfYUV420P8  },
        { pix_or_depth(VideoInfo::CS_YV12,  9), pfYUV420P9  },
        { pix_or_depth(VideoInfo::CS_YV12, 10), pfYUV420P10 },
        { pix_or_depth(VideoInfo::CS_YV12, 16), pfYUV420P16 },
        { pix_or_depth(VideoInfo::CS_Y8,    8), pfGray8     },
        { pix_or_depth(VideoInfo::CS_Y8,   16), pfGray16    },
        { val, 0 }
    };

    int i;
    for (i = 0; table[i].avs_pix_type != val; i++);
    validate(table[i].id == 0, "couldn't found valid format type");

    return api->getFormatPreset(table[i].id, core);
}


template <bool ALPHA, int CHANNELS>
static void __stdcall
write_rgb(VSFrameRef** dsts, PVideoFrame& src, int, const VSAPI* api) noexcept
{
    VSFrameRef* dst = dsts[0];

    const int spitch = src->GetPitch();
    const int dstride = api->getStride(dst, 0);
    const int width = src->GetRowSize() / CHANNELS;
    const int height = src->GetHeight();

    const uint8_t* srcp = src->GetReadPtr() + spitch * (height - 1);
    uint8_t* dstpr = api->getWritePtr(dst, 0);
    uint8_t* dstpg = api->getWritePtr(dst, 1);
    uint8_t* dstpb = api->getWritePtr(dst, 2);
    uint8_t* dstpa = ALPHA ? api->getWritePtr(dsts[1], 0) : nullptr;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            dstpb[x] = srcp[CHANNELS * x + 0];
            dstpg[x] = srcp[CHANNELS * x + 1];
            dstpr[x] = srcp[CHANNELS * x + 2];
            if (ALPHA) {
                dstpa[x] = srcp[4 * x + 3];
            }
        }
        srcp -= spitch;
        dstpr += dstride;
        dstpg += dstride;
        dstpb += dstride;
        if (ALPHA) {
            dstpa += dstride;
        }
    }
}


static void __stdcall
write_yuv(VSFrameRef** dsts, PVideoFrame& src, int num_planes,
          const VSAPI* api) noexcept
{
    static const int planes[] = { PLANAR_Y, PLANAR_U, PLANAR_V };

    VSFrameRef* dst = dsts[0];

    for (int i = 0; i < num_planes; ++i) {
        int plane = planes[i];
        bitblt(api->getWritePtr(dst, i), api->getStride(dst, i),
               src->GetReadPtr(plane), src->GetPitch(plane),
               src->GetRowSize(plane), src->GetHeight(plane));
    }
}


AvsReader::~AvsReader()
{
    AVS_linkage = nullptr;
    if (env) {
        env->DeleteScriptEnvironment();
        env = nullptr;
    }
    if (dll) {
        FreeLibrary(dll);
        dll = nullptr;
    }
}


AvsReader::AvsReader(HMODULE d, ise_t* e, PClip c, int n, int bit_depth,
                     VSCore* core, const VSAPI* api) :
    dll(d), env(e), clip(c), numOutputs(n)
{
    viAVS = clip->GetVideoInfo();

    vi[0].format = get_vs_format(viAVS.pixel_type, bit_depth, core, api);
    vi[0].fpsNum = viAVS.fps_numerator;
    vi[0].fpsDen = viAVS.fps_denominator;
    vi[0].width = bit_depth > 8 ? viAVS.width / 2 : viAVS.width;
    vi[0].height = viAVS.height;
    vi[0].numFrames = viAVS.num_frames;

    if (numOutputs == 2) {
        vi[1] = vi[0];
        vi[1].format = api->getFormatPreset(pfGray8, core);
        write_frame = write_rgb<true, 4>;
    } else if (viAVS.IsRGB32()) {
        write_frame = write_rgb<false, 4>;
    } else if (viAVS.IsRGB24()) {
        write_frame = write_rgb<false, 3>;
    } else {
        write_frame = write_yuv;
    }
}


const VSFrameRef* __stdcall AvsReader::
getFrame(int n, VSCore* core, const VSAPI* api, VSFrameContext* ctx)
{
    n = std::min(std::max(n, 0), viAVS.num_frames - 1);

    VSFrameRef* dsts[2] = {
        api->newVideoFrame(vi[0].format, vi[0].width, vi[0].height, nullptr,
                           core),
        nullptr
    };

    VSMap *props = api->getFramePropsRW(dsts[0]);
    api->propSetInt(props, "_DurationNum", viAVS.fps_denominator, paReplace);
    api->propSetInt(props, "_DurationDen", viAVS.fps_numerator, paReplace);

    PVideoFrame src = clip->GetFrame(n, env);

    if (numOutputs == 1) {
        write_frame(dsts, src, vi[0].format->numPlanes, api);
        return dsts[0];
    }

    dsts[1] = api->newVideoFrame(vi[1].format, vi[1].width, vi[1].height,
                                nullptr, core);
    props = api->getFramePropsRW(dsts[1]);
    api->propSetInt(props, "_DurationNum", viAVS.fps_denominator, paReplace);
    api->propSetInt(props, "_DurationDen", viAVS.fps_numerator, paReplace);

    write_frame(dsts, src, 1, api);

    return api->cloneFrameRef(dsts[api->getOutputIndex(ctx)]);
}



AvsReader* AvsReader::
create(const char* input, int bit_depth, bool alpha, const char* mode,
       VSCore* core, const VSAPI* api)
{
    typedef ise_t* (__stdcall *cse_t)(int);

    HMODULE dll = nullptr;
    ise_t* env = nullptr;

    try {
        dll = LoadLibrary("avisynth");
        validate(!dll, "failed to load avisynth.dll");

        cse_t create_env = reinterpret_cast<cse_t>(
            GetProcAddress(dll, "CreateScriptEnvironment"));
        validate(!create_env, "failed to load CreateScriptEnvironment().");

        env = create_env(AVISYNTH_INTERFACE_VERSION);
        validate(!env, "failed to create avisynth script environment.");
        AVS_linkage = env->GetAVSLinkage();

        std::vector<char> ansi;
        convert_utf8_to_ansi(input, ansi);
        AVSValue res = env->Invoke(mode, AVSValue(ansi.data()));
        validate(!res.IsClip(), "failed to evaluate avs clip.");

        PClip clip = res.AsClip();
        const VideoInfo& vi = clip->GetVideoInfo();
        validate(!vi.HasVideo(), "avs clip has no video.");

        if (bit_depth > 8) {
            validate(!vi.IsPlanar() || vi.IsYV411() || (vi.width & 1)
                     || (vi.IsY8() && bit_depth != 16)
                     || ((vi.IsYV16() || vi.IsYV12()) && (vi.width & 3)),
                     "invalid bitdepth or resolution");
        }

        if (vi.IsYUY2()) {
            clip = env->Invoke("ConvertToYV16", clip).AsClip();
        }

        int outputs = vi.IsRGB32() && alpha ? 2 : 1;

        return new AvsReader(dll, env, clip, outputs, bit_depth, core, api);

    } catch (std::string e) {
        AVS_linkage = nullptr;
        if (env) {
            env->DeleteScriptEnvironment();
        }
        if (dll) {
            FreeLibrary(dll);
        }
        throw e;
    } catch (AvisynthError e) {
        auto msg = std::string(e.msg);
        AVS_linkage = nullptr;
        env->DeleteScriptEnvironment();
        FreeLibrary(dll);
        throw msg;
    }

    return nullptr;
}
