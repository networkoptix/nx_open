// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "hw_video_api.h"

namespace nx::media {

VideoApiRegistry* VideoApiRegistry::instance()
{
    static VideoApiRegistry instance;
    return &instance;
}

void VideoApiRegistry::add(VideoApiRegistry::Entry* entry)
{
    m_entries.insert(entry->deviceType(), entry);
}

VideoApiRegistry::Entry* VideoApiRegistry::get(AVHWDeviceType deviceType)
{
    return m_entries.value(deviceType, nullptr);
}

VideoApiRegistry::Entry* VideoApiRegistry::get(const AVFrame* frame)
{
    if (!frame)
        return nullptr;

    // Some HW decoders do not use hw_frames_ctx.
    switch (frame->format)
    {
        case AV_PIX_FMT_MEDIACODEC:
            return get(AV_HWDEVICE_TYPE_MEDIACODEC);
        case AV_PIX_FMT_VIDEOTOOLBOX:
            return get(AV_HWDEVICE_TYPE_VIDEOTOOLBOX);
        default:
            break;
    }

    if (!frame->hw_frames_ctx)
        return nullptr;

    auto hwFramesContext = (AVHWFramesContext*) frame->hw_frames_ctx->data;
    return hwFramesContext ? get(hwFramesContext->device_ctx->type) : nullptr;
}

bool VideoApiRegistry::useLegacyMediaCodec = false;

void VideoApiRegistry::enableLegacyMediaCodec()
{
    useLegacyMediaCodec = true;
}

VideoApiRegistry::VideoApiRegistry()
{
    static std::once_flag registerEntries;
    std::call_once(registerEntries,
        [this]()
        {
            #if defined(Q_OS_APPLE)
                add(getVideoToolboxApi());
            #elif defined(Q_OS_WIN)
                add(getD3D11Api());
                add(getD3D12Api());
            #elif defined(Q_OS_ANDROID)
                add(useLegacyMediaCodec ? getMediaCodecApi() : getNdkMediaCodecApi());
            #elif defined(Q_OS_LINUX) && defined(__x86_64__)
                add(getVaApiApi());
            #endif

            #if QT_CONFIG(vulkan) && !defined(Q_OS_ANDROID)
                add(getVulkanApi());
            #endif
        });
}

} // namespace nx::media
