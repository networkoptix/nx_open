#pragma once

#ifndef DISABLE_FFMPEG

#include <QtCore/QObject>

#include <nx/streaming/audio_data_packet.h>

#include "abstract_audio_decoder.h"

namespace nx {
namespace media {

class FfmpegAudioDecoderPrivate;

/**
 * Implements ffmpeg audio decoder.
 */
class FfmpegAudioDecoder: public AbstractAudioDecoder
{
public:
    FfmpegAudioDecoder();
    virtual ~FfmpegAudioDecoder();

    static bool isCompatible(const CodecID codec);
    virtual AudioFramePtr decode(const QnConstCompressedAudioDataPtr& frame) override;

private:
    QScopedPointer<FfmpegAudioDecoderPrivate> d_ptr;
    Q_DECLARE_PRIVATE(FfmpegAudioDecoder);
};

} // namespace media
} // namespace nx

#endif // #DISABLE_FFMPEG
