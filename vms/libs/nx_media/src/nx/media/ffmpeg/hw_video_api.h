// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtGui/rhi/qrhi.h>

#include <nx/media/media_fwd.h>
#include <nx/media/ffmpeg/hw_video_decoder.h>

extern "C" {
#include <libavutil/hwcontext.h>
} // extern "C"

struct AVFrame;

namespace nx::media {

class VideoApiDecoderData
{
public:
    VideoApiDecoderData(QRhi* rhi): m_rhi(rhi) {}
    virtual ~VideoApiDecoderData() = default;

    QRhi* rhi() const { return m_rhi; }

private:
    QRhi* const m_rhi;
};

class VideoApiRegistry
{
    VideoApiRegistry();
public:
    class Entry
    {
    public:
        virtual ~Entry() = default;

        virtual AVHWDeviceType deviceType() const = 0;
        virtual nx::media::VideoFramePtr makeFrame(
            const AVFrame* frame,
            std::shared_ptr<VideoApiDecoderData> decoderData) const = 0;
        virtual nx::media::ffmpeg::HwVideoDecoder::InitFunc initFunc(
            [[maybe_unused]] QRhi* rhi,
            [[maybe_unused]] std::shared_ptr<VideoApiDecoderData>decoderData) const
        {
            return {};
        }
        virtual std::unique_ptr<nx::media::ffmpeg::AvOptions> options(QRhi*) const { return {}; }
        virtual std::string device(QRhi*) const { return {}; }
        virtual std::shared_ptr<VideoApiDecoderData> createDecoderData(QRhi*) const { return {}; }
    };

public:
    static VideoApiRegistry* instance();

    VideoApiRegistry::Entry* get(AVHWDeviceType deviceType);
    VideoApiRegistry::Entry* get(const AVFrame* frame);

private:
    void add(VideoApiRegistry::Entry* entry);
    QHash<AVHWDeviceType, Entry*> m_entries;
};

#if defined(Q_OS_APPLE)
    VideoApiRegistry::Entry* getVideoToolboxApi();
#elif defined(Q_OS_WIN)
    VideoApiRegistry::Entry* getD3D11Api();
    VideoApiRegistry::Entry* getD3D12Api();
#elif defined(Q_OS_ANDROID)
    VideoApiRegistry::Entry* getMediaCodecApi();
#elif defined(Q_OS_LINUX) && defined(__x86_64__)
    VideoApiRegistry::Entry* getVaApiApi();
#endif

#if QT_CONFIG(vulkan) && !defined(Q_OS_ANDROID)
    VideoApiRegistry::Entry* getVulkanApi();
#endif

} // namespace nx::media
