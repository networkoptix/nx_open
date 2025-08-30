// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/media/video_data_packet.h>

#include "abstract_video_decoder.h"

class QRhi;

namespace nx {
namespace media {

class JpegDecoderPrivate;

/**
 * Implements software JPEG video decoder either via libJpeg (if exists) or QT.
 */
class NX_MEDIA_API JpegDecoder: public AbstractVideoDecoder
{
public:
    JpegDecoder(const QSize& resolution, QRhi* rhi);
    virtual ~JpegDecoder() override;

    static bool isCompatible(
        const AVCodecID codec,
        const QSize& resolution,
        bool allowHardwareAcceleration);

    static QSize maxResolution(const AVCodecID codec);

    virtual bool sendPacket(const QnConstCompressedVideoDataPtr& packet) override;
    virtual bool receiveFrame(VideoFramePtr* decodedFrame) override;
    virtual int currentFrameNumber() const override;

    virtual Capabilities capabilities() const override;
private:
    QScopedPointer<JpegDecoderPrivate> d_ptr;
    Q_DECLARE_PRIVATE(JpegDecoder);
};

} // namespace media
} // namespace nx
