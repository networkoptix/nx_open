#include "abstract_audio_decoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include "ffmpeg_audio_decoder.h"

QnAbstractAudioDecoder* QnAudioDecoderFactory::createDecoder(QnCompressedAudioDataPtr data)
{
    return new QnFfmpegAudioDecoder(data);
}

#endif // ENABLE_DATA_PROVIDERS
