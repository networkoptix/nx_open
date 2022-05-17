// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>

#include <nx/streaming/video_data_packet.h>

#include "abstract_video_decoder.h"

namespace nx {
namespace media {

class JpegDecoderPrivate;

/**
 * Implements software JPEG video decoder either via libJpeg (if exists) or QT.
 */
class JpegDecoder: public AbstractVideoDecoder
{
public:
    JpegDecoder(const RenderContextSynchronizerPtr& synchronizer, const QSize& resolution);

    static bool isCompatible(
        const AVCodecID codec,
        const QSize& resolution,
        bool allowOverlay,
        bool allowHardwareAcceleration);

    static QSize maxResolution(const AVCodecID codec);

    virtual int decode(
        const QnConstCompressedVideoDataPtr& frame, QVideoFramePtr* result = nullptr) override;

    virtual Capabilities capabilities() const override;
private:
    QScopedPointer<JpegDecoderPrivate> d_ptr;
    Q_DECLARE_PRIVATE(JpegDecoder);
};

} // namespace media
} // namespace nx
