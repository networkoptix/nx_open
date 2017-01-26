#pragma once

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

    static bool isCompatible(const AVCodecID codec);
    virtual bool decode(const QnConstCompressedAudioDataPtr& frame, AudioFramePtr* const outFrame) override;

private:
    QScopedPointer<FfmpegAudioDecoderPrivate> d_ptr;
    Q_DECLARE_PRIVATE(FfmpegAudioDecoder);
};

} // namespace media
} // namespace nx
