#include "abstractaudiodecoder.h"
#include "ffmpeg_audio.h"



CLAbstractAudioDecoder* CLAudioDecoderFactory::createDecoder(CLCodecType codec, void* codecContext)
{
	return new CLFFmpegAudioDecoder(codec, (AVCodecContext*)codecContext);
}

