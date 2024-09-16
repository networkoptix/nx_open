// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "hw_video_api.h"

namespace nx::media {

VideoApiRegistry* VideoApiRegistry::instance()
{
    static VideoApiRegistry instance;
    return &instance;
}

void VideoApiRegistry::add(AVHWDeviceType deviceType, VideoApiRegistry::Entry* entry)
{
    m_entries.insert(deviceType, entry);
}

VideoApiRegistry::Entry* VideoApiRegistry::get(AVHWDeviceType deviceType)
{
    return m_entries.value(deviceType, nullptr);
}

VideoApiRegistry::Entry* VideoApiRegistry::get(const AVFrame* frame)
{
    if (!frame || !frame->hw_frames_ctx)
        return nullptr;

    auto hwFramesContext = (AVHWFramesContext*) frame->hw_frames_ctx->data;
    return hwFramesContext ? get(hwFramesContext->device_ctx->type) : nullptr;
}

} // namespace nx::media
