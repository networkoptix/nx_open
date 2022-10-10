// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nvidia_renderer.h"

#include <nx/utils/log/log.h>
#include <nx/media/nvidia/nvidia_video_frame.h>
#include <nx/media/nvidia/nvidia_video_decoder.h>
#include <nx/media/nvidia/renderer.h>

namespace nx::media::nvidia {

bool renderToRgb(AbstractVideoSurface* frame, GLuint textureId, const QSize& textureSize)
{
    NvidiaVideoFrame* nvFrame = dynamic_cast<NvidiaVideoFrame*>(frame);
    if (!nvFrame)
        return false;

    auto decoderLock = nvFrame->decoder.lock();
    if (!decoderLock)
    {
        NX_DEBUG(NX_SCOPE_TAG, "Skip frame rendering, decoder has destroyed already");
        return false;
    }
    decoderLock->pushContext();
    bool result = decoderLock->getRenderer().render(textureId, textureSize, *nvFrame);
    decoderLock->popContext();
    return result;
}

} // namespace nx::media::nvidia
