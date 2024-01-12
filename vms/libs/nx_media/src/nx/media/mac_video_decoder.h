// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once
#include <QtCore/QtGlobal>
#if defined(Q_OS_IOS) || defined(Q_OS_MACOS)

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>

#include <nx/media/video_data_packet.h>

#include "abstract_video_decoder.h"

namespace nx {
namespace media {

class MacVideoDecoderPrivate;

/**
 * Implements ffmpeg video decoder.
 */
class MacVideoDecoder: public AbstractVideoDecoder
{
public:
    MacVideoDecoder(const RenderContextSynchronizerPtr& synchronizer, const QSize& resolution);
    virtual ~MacVideoDecoder();

    static bool isCompatible(
        const AVCodecID codec,
        const QSize& resolution,
        bool allowOverlay,
        bool allowHardwareAcceleration);

    static QSize maxResolution(const AVCodecID codec);

    virtual int decode(
        const QnConstCompressedVideoDataPtr& frame, VideoFramePtr* result = nullptr) override;

    virtual Capabilities capabilities() const override;
private:
    void ffmpegToQtVideoFrame(VideoFramePtr* result);

private:
    QScopedPointer<MacVideoDecoderPrivate> d_ptr;
    Q_DECLARE_PRIVATE(MacVideoDecoder);
};

} // namespace media
} // namespace nx

#endif // Q_OS_IOS
