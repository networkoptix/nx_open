#pragma once

#include <QtMultimedia/QVideoFrame>

#include <nx/streaming/video_data_packet.h>
#include <nx/media/abstract_video_decoder.h>

namespace nx::media {

class QuickSyncVideoDecoderImpl;

class QuickSyncVideoDecoder: public AbstractVideoDecoder
{
public:
    struct Config
    {
        bool useVideoMemory = true;
    };

public:
    QuickSyncVideoDecoder(
        const RenderContextSynchronizerPtr& /*synchronizer*/, const QSize& /*resolution*/);
    ~QuickSyncVideoDecoder();

    static bool isCompatible(const AVCodecID codec, const QSize& resolution, bool allowOverlay);
    static QSize maxResolution(const AVCodecID codec);

    virtual int decode(
        const QnConstCompressedVideoDataPtr& frame, QVideoFramePtr* result = nullptr) override;

    virtual Capabilities capabilities() const override;

private:
    std::unique_ptr<QuickSyncVideoDecoderImpl> m_impl;
};

} // namespace nx::media
