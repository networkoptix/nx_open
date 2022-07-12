// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nvcuvid.h>

namespace nx::media::nvidia {

inline std::string toString(CUresult error)
{
    const char *szErrName = NULL;
    cuGetErrorName(error, &szErrName);
    return std::string(szErrName);
}

} // namespace nx::media::nvidia
