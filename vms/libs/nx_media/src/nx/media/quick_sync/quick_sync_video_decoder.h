// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/media/abstract_video_decoder.h>
#include <nx/media/video_data_packet.h>

class QRhi;

namespace nx::media::quick_sync {

class QuickSyncVideoDecoderImpl;

class NX_MEDIA_API QuickSyncVideoDecoder: public AbstractVideoDecoder
{
public:
    struct Config
    {
        bool useVideoMemory = true;
    };

public:
    QuickSyncVideoDecoder(const QSize& /*resolution*/, QRhi*);
    ~QuickSyncVideoDecoder();

    static bool isCompatible(
        const AVCodecID codec,
        const QSize& resolution,
        bool allowHardwareAcceleration);
    static bool isAvailable();
    static QSize maxResolution(const AVCodecID codec);

    int decode(
        const QnConstCompressedVideoDataPtr& frame, VideoFramePtr* result = nullptr);

    virtual bool sendPacket(const QnConstCompressedVideoDataPtr& packet) override;
    virtual bool receiveFrame(VideoFramePtr* decodedFrame) override;
    virtual int currentFrameNumber() const override;

    virtual Capabilities capabilities() const override;

private:
    // Should be shared due to output frames keep reference to decoder.
    std::shared_ptr<QuickSyncVideoDecoderImpl> m_impl;
    VideoFramePtr m_result;
    int m_frameNumber = 0;
};

} // namespace nx::media::quick_sync
