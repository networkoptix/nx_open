// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nvidia_video_frame.h"

#include <nx/media/nvidia/nvidia_video_decoder.h>

namespace nx::media::nvidia {

AVFrame NvidiaVideoFrame::lockFrame()
{
    AVFrame result;
    return result;
}

void NvidiaVideoFrame::unlockFrame()
{
}

NvidiaVideoFrame::~NvidiaVideoFrame()
{
    auto decoderLock = decoder.lock();
    if (decoderLock)
        decoderLock->releaseFrame(frameData);
}

} // namespace nx::media::nvidia