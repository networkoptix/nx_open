// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nvidia_video_frame.h"

#include <cuda.h>
#include <cuda_runtime.h>

#include <nx/media/nvidia/nvidia_video_decoder.h>
#include <nx/utils/log/log.h>

namespace nx::media::nvidia {

AVFrame NvidiaVideoFrame::lockFrame()
{
    AVFrame result;
    memset(&result, 0, sizeof(AVFrame));
    auto decoderLock = decoder.lock();
    if (!decoderLock)
    {
        NX_DEBUG(this, "Nvidia decoder already deleted, failed to copy frame to system memory");
        return result;
    }
    systemMemory.resize(bufferSize);
    cudaMemcpy(systemMemory.data(), frameData, bufferSize, cudaMemcpyDeviceToHost);

    result.width = width;
    result.height = height;
    result.linesize[0] = pitch;
    result.linesize[1] = pitch;
    result.linesize[2] = 0;
    result.linesize[3] = 0;
    result.data[0] = systemMemory.data();
    result.data[1] = systemMemory.data() + pitch * height;
    result.data[2] = nullptr;
    result.data[3] = nullptr;
    result.format = AV_PIX_FMT_NV12;
    return result;
}

void NvidiaVideoFrame::unlockFrame()
{
    systemMemory.clear();
}

NvidiaVideoFrame::~NvidiaVideoFrame()
{
    auto decoderLock = decoder.lock();
    if (decoderLock)
        decoderLock->releaseFrame(frameData);
}

} // namespace nx::media::nvidia
