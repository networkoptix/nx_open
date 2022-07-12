// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nvidia_video_frame.h"

#include <nvcuvid.h>

#include <nx/utils/log/log.h>

#include <nx/media/nvidia/NvCodecUtils.h>
#include <nx/media/nvidia/linux/renderer.h>
#include <nx/media/nvidia/nvidia_video_decoder.h>

bool NvidiaVideoFrame::renderToRgb(
    bool isNewTexture,
    GLuint textureId,
    [[maybe_unused]] int scaleFactor,
    float* cropWidth,
    float* cropHeight)
{
    auto decoderLock = decoder.lock();
    if (!decoderLock)
    {
        NX_DEBUG(this, "Skip frame rendering, decoder has destroyed already");
        return false;
    }

    if (cropWidth)
        *cropWidth = 0;
    if (cropHeight)
        *cropHeight = 0;
   return decoderLock->getRenderer().render(
       isNewTexture, textureId, frameData, (cudaVideoSurfaceFormat)format, bitDepth, matrix);
}

AVFrame NvidiaVideoFrame::lockFrame()
{
    AVFrame result;
    return result;
}

void NvidiaVideoFrame::unlockFrame()
{
}

