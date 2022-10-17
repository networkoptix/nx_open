// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nvcuvid.h>

#include <nx/media/nvidia/nvidia_driver_proxy.h>

namespace nx::media::nvidia {

inline std::string toString(CUresult error)
{
    const char *szErrName = nullptr;
    NvidiaDriverApiProxy::instance().cuGetErrorName(error, &szErrName);
    return std::string(szErrName);
}

inline bool isNvidiaAvailable()
{
    if (!NvidiaDriverApiProxy::instance().load())
        return false;

    CUresult status = NvidiaDriverApiProxy::instance().cuInit(0);
    if (status != CUDA_SUCCESS)
        return false;

    return true;
}

} // namespace nx::media::nvidia
