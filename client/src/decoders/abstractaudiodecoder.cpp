#include "abstractaudiodecoder.h"

#include "ffmpeg/ffmpegaudiodecoder_p.h"

CLAbstractAudioDecoder::CLAbstractAudioDecoder()
    : m_lastError(NoError)
{
}

CLAbstractAudioDecoder::~CLAbstractAudioDecoder()
{
}


CLAbstractAudioDecoder *CLAudioDecoderFactory::createDecoder(QnCompressedAudioDataPtr data)
{
    return new CLFFmpegAudioDecoder(data);
}
