/*
AvsReader.h

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


#ifndef VS_AVS_READER_H
#define VS_AVS_READER_H

#include <string>
#define WIN32_LEAN_AND_MEAN
#define VS_EXTRALEAN
#define NOMINMAX
#define NOGDI
#include <windows.h>
#include <avisynth.h>
#include <VapourSynth.h>


#define VSAVSREADER_VERSION "1.0.0"


class AvsReader {

    typedef IScriptEnvironment ise_t;

    HMODULE dll;
    ise_t* env;
    PClip clip;
    VideoInfo viAVS;
    VSVideoInfo vi[2];
    int numOutputs;

    void (__stdcall *write_frame)(
        VSFrameRef** dst, PVideoFrame& src, int num_planes, const VSAPI* api);

    AvsReader(HMODULE dll, ise_t* env, PClip clip, int outputs, int bit_depth,
              VSCore* core, const VSAPI* api);

public:
    ~AvsReader();
    const VSFrameRef* __stdcall getFrame(int n, VSCore* core, const VSAPI* api,
                                         VSFrameContext* ctx);
    const VSVideoInfo* getVSVideoInfo() { return vi; }
    const int getNumOutputs() { return numOutputs; }
    static AvsReader* create(const char* input, int bit_depth, bool alpha,
                             const char* mode, VSCore* core, const VSAPI* api);
};


static inline void validate(bool cond, const char* msg)
{
    if (cond) {
        throw std::string(msg);
    }
}

#endif

