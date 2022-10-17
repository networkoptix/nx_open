// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/streaming/video_data_packet.h>
#include <nx/media/abstract_video_decoder.h>

namespace nx::media::quick_sync {

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
    static bool isAvailable();
    static QSize maxResolution(const AVCodecID codec);

    virtual int decode(
        const QnConstCompressedVideoDataPtr& frame, QVideoFramePtr* result = nullptr) override;

    virtual Capabilities capabilities() const override;

private:
    // Should be shared due to output frames keep reference to decoder.
    std::shared_ptr<QuickSyncVideoDecoderImpl> m_impl;
};

} // namespace nx::media::quick_sync
