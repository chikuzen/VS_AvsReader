/*
plugin.cpp

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


#include "AvsReader.h"
#include "myvshelper.h"


static const VSFrameRef* VS_CC
get_frame(int n, int activation_reason, void **instance_data, void **,
          VSFrameContext *frame_ctx, VSCore *core, const VSAPI *api)
{
    if (activation_reason != arInitial) {
        return nullptr;
    }

    auto d = reinterpret_cast<AvsReader*>(*instance_data);
    try {
        return d->getFrame(n, core, api, frame_ctx);
    } catch (AvisynthError e) {
        api->setFilterError(e.msg, frame_ctx);
    }
    return nullptr;
}


static void VS_CC
free_filter(void* instance_data, VSCore*, const VSAPI*)
{
    delete reinterpret_cast<AvsReader*>(instance_data);
}


static void VS_CC
init_filter(VSMap*, VSMap*, void** instance_data, VSNode* node, VSCore*,
            const VSAPI* api)
{
    auto d = reinterpret_cast<AvsReader*>(*instance_data);
    api->setVideoInfo(d->getVSVideoInfo(), d->getNumOutputs(), node);
}


static void VS_CC
create_avsr(const VSMap* in, VSMap* out, void* user_data, VSCore* core,
              const VSAPI* api)
{
    const char* mode = reinterpret_cast<char*>(user_data);

    int bd = get_arg("bitdepth", 8, 0, in, api);

    const char* input =
        get_arg(mode[0] == 'E' ? "lines" : "script", "", 0, in, api);

    bool alpha = get_arg("alpha", true, 0, in, api);

    try {
        validate(bd != 8 && bd != 9 && bd != 10 && bd != 16,
                 "invalid bitdepth was specified.");
        validate(strlen(input) < 1, "zero length avs.");

        auto d = AvsReader::create(input, bd, alpha, mode, core, api);

        api->createFilter(in, out, mode, init_filter, get_frame, free_filter,
                          fmSerial, 0, d, core);

    } catch (std::string& e) {
        auto msg = std::string(mode) + ": " + e;
        api->setError(out, msg.c_str());
    }
}


extern "C" __declspec(dllexport) void VS_CC
VapourSynthPluginInit(VSConfigPlugin conf, VSRegisterFunction reg, VSPlugin* p)
{
    conf("chikuzen.does.not.have.his.own.domain.avsr", "avsr",
         "AviSynth Script Reader for VapourSynth v" VSAVSREADER_VERSION,
         VAPOURSYNTH_API_VERSION, 1, p);
    reg("Import", "script:data;bitdepth:int:opt;alpha:int:opt;", create_avsr,
        "Import", p);
    reg("Eval", "lines:data;bitdepth:int:opt;alpha:int:opt;", create_avsr,
        "Eval", p);
}

