// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/media/video_data_packet.h>

#include "abstract_video_decoder.h"

class QRhi;

namespace nx::media {

/**
 * Implements ffmpeg-based hardware video decoder.
 */
class FfmpegHwVideoDecoder
:
    public AbstractVideoDecoder
{
public:
    FfmpegHwVideoDecoder(const QSize& resolution, QRhi* rhi);
    virtual ~FfmpegHwVideoDecoder();

    static bool isCompatible(
        const AVCodecID codec,
        const QSize& resolution,
        bool allowHardwareAcceleration);

    static QSize maxResolution(const AVCodecID codec);

    /**
     * Whether the process-wide back-off armed by a recent hardware decoder resource failure
     * is currently active. While active, isCompatible() returns false and newly created
     * decoders fall back to software.
     */
    static bool isHardwareTemporarilyUnavailable();

    virtual bool sendPacket(const QnConstCompressedVideoDataPtr& packet) override;
    virtual bool receiveFrame(VideoFramePtr* decodedFrame) override;
    virtual int currentFrameNumber() const override;

    virtual Capabilities capabilities() const override;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::media
