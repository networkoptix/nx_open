// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nvidia_renderer.h"

#include <nx/media/nvidia/nvidia_video_frame.h>

bool renderToRgb(AbstractVideoSurface* frame, GLuint textureId, int textureWidth, int textureHeight)
{
    NvidiaVideoFrame* nvFrame = dynamic_cast<NvidiaVideoFrame*>(frame);
    if (!nvFrame)
        return false;

    return nvFrame->renderToRgb(textureId, textureWidth, textureHeight);
}