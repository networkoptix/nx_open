// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>

#include <nx/media/media_fwd.h>
#include <nx/media/ffmpeg/hw_video_decoder.h>

extern "C" {
#include <libavutil/hwcontext.h>
} // extern "C"

struct AVFrame;
class QRhi;

namespace nx::media {

class VideoApiRegistry
{
    VideoApiRegistry() = default;
public:
    class Entry
    {
    public:
        virtual ~Entry() = default;

        virtual AVHWDeviceType deviceType() const = 0;
        virtual nx::media::VideoFramePtr makeFrame(const AVFrame* frame) const = 0;
        virtual nx::media::ffmpeg::HwVideoDecoder::InitFunc initFunc(QRhi*) const { return {}; }
        virtual std::unique_ptr<nx::media::ffmpeg::AvOptions> options(QRhi*) const { return {}; }
    };

public:
    static VideoApiRegistry* instance();

    void add(AVHWDeviceType deviceType, VideoApiRegistry::Entry* entry);
    VideoApiRegistry::Entry* get(AVHWDeviceType deviceType);
    VideoApiRegistry::Entry* get(const AVFrame* frame);

private:
    QHash<AVHWDeviceType, Entry*> m_entries;
};

// nx::media::VideoFramePtr makeVideoFrame(AVFrame* avFrame);

} // namespace nx::media
