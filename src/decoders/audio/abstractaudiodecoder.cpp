#include "abstractaudiodecoder.h"
#include "ffmpeg_audio.h"

CLAbstractAudioDecoder* CLAudioDecoderFactory::createDecoder(CodecID codecId, void* codecContext)
{
	return new CLFFmpegAudioDecoder(codecId, (AVCodecContext*)codecContext);
}

