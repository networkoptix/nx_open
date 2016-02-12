#pragma once

#ifndef DISABLE_FFMPEG

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>

#include <nx/streaming/audio_data_packet.h>

#include "abstract_audio_decoder.h"

namespace nx {
namespace media {

/*
* This class implements ffmpeg video decoder
*/
class FfmpegAudioDecoderPrivate;
class FfmpegAudioDecoder : public AbstractAudioDecoder
{
public:
    FfmpegAudioDecoder();
    virtual ~FfmpegAudioDecoder();

    static bool isCompatible(const CodecID codec);
    virtual QnAudioFramePtr decode(const QnConstCompressedAudioDataPtr& frame) override;
private:
    QScopedPointer<FfmpegAudioDecoderPrivate> d_ptr;
    Q_DECLARE_PRIVATE(FfmpegAudioDecoder);
};

}
}

#endif // #DISABLE_FFMPEG
