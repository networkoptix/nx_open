#pragma once

#ifndef DISABLE_FFMPEG

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>

#include <nx/streaming/video_data_packet.h>

#include "abstract_video_decoder.h"

namespace nx {
namespace media {

class FfmpegVideoDecoderPrivate;

/**
 * Implements ffmpeg video decoder.
 */
class FfmpegVideoDecoder: public AbstractVideoDecoder
{
public:
    FfmpegVideoDecoder();
    virtual ~FfmpegVideoDecoder();

    static bool isCompatible(const CodecID codec, const QSize& resolution);
    virtual int decode(
        const QnConstCompressedVideoDataPtr& frame, QVideoFramePtr* result = nullptr) override;

private:
    void ffmpegToQtVideoFrame(QVideoFramePtr* result);

private:
    QScopedPointer<FfmpegVideoDecoderPrivate> d_ptr;
    Q_DECLARE_PRIVATE(FfmpegVideoDecoder);
};

} // namespace media
} // namespace nx

#endif // #DISABLE_FFMPEG
