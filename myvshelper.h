/*
myvshelper.h

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


#ifndef MY_VAPOURSYNTH_HELPER_H
#define MY_VAPOURSYNTH_HELPER_H

#include <cstdint>
#include <cstdlib>
#include <VapourSynth.h>


template <typename T>
T get_prop(const VSAPI*, const VSMap*, const char*, int, int*);

template <>
static inline int
get_prop<int>(const VSAPI* api, const VSMap* in, const char* name, int idx,
              int* e)
{
    return static_cast<int>(api->propGetInt(in, name, idx, e));
}

template <>
static inline int64_t
get_prop<int64_t>(const VSAPI* api, const VSMap* in, const char* name, int idx,
                  int* e)
{
    return api->propGetInt(in, name, idx, e);
}

template <>
static inline bool
get_prop<bool>(const VSAPI* api, const VSMap* in, const char* name, int idx,
               int* e)
{
    return api->propGetInt(in, name, idx, e) != 0;
}

template <>
static inline float
get_prop<float>(const VSAPI* api, const VSMap* in, const char* name, int idx,
                int* e)
{
    return static_cast<float>(api->propGetFloat(in, name, idx, e));
}

template <>
static inline double
get_prop<double>(const VSAPI* api, const VSMap* in, const char* name, int idx,
                 int* e)
{
    return api->propGetFloat(in, name, idx, e);
}

template <>
static inline const char*
get_prop<const char*>(const VSAPI* api, const VSMap* in, const char* name, int idx,
                      int* e)
{
    return api->propGetData(in, name, idx, e);
}

template <typename T>
static inline T get_arg(const char* name, T default_value, int index,
                        const VSMap* in, const VSAPI* api)
{
    int err = 0;
    T ret = get_prop<T>(api, in, name, index, &err);
    if (err) {
        ret = default_value;
    }
    return ret;
}

template <typename T>
static inline void
bitblt(T* dstp, const int dstride, const T* srcp, const int sstride,
       const size_t width, const size_t height)
{
    if (height == 0) {
        return;
    }
    const size_t w = width * sizeof(T);
    if (sstride == dstride && sstride == static_cast<int>(width)) {
        memcpy(dstp, srcp, w * height);
        return;
    }
    for (size_t y = 0; y < height; ++y) {
        memcpy(dstp, srcp, w);
        srcp += sstride;
        dstp += dstride;
    }
}


#endif
