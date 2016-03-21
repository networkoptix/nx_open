#include "abstractaudiodecoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include "ffmpeg_audio.h"

CLAbstractAudioDecoder* CLAudioDecoderFactory::createDecoder(QnCompressedAudioDataPtr data)
{
    return new CLFFmpegAudioDecoder(data);
}

#endif // ENABLE_DATA_PROVIDERS
