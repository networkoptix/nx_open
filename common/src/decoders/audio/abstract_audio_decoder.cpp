#include "abstract_audio_decoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include "ffmpeg_audio_decoder.h"

QnAbstractAudioDecoder* QnAudioDecoderFactory::createDecoder(QnCompressedAudioDataPtr data)
{
    auto result = new QnFfmpegAudioDecoder(data);
    if (!result->isInitialized())
    {
        delete result;
        return nullptr;
    }
    return result;
}

#endif // ENABLE_DATA_PROVIDERS
