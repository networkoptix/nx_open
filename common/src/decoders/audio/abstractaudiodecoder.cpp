#include "abstractaudiodecoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include "ffmpeg_audio.h"

CLAbstractAudioDecoder* CLAudioDecoderFactory::createDecoder(QnCompressedAudioDataPtr data)
{
    auto result = new CLFFmpegAudioDecoder(data);
    if (!result->isInitialized())
    {
        delete result;
        return nullptr;
    }
    return result;
}

#endif // ENABLE_DATA_PROVIDERS
