#include "abstractaudiodecoder.h"
#include "ffmpeg_audio.h"



CLAbstractAudioDecoder* CLAudioDecoderFactory::createDecoder(CLCodecType codec)
{
	return new CLFFmpegAudioDecoder(codec);
}

