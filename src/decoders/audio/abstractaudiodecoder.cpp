#include "abstractaudiodecoder.h"
#include "ffmpeg_audio.h"


CLAbstractAudioDecoder* CLAudioDecoderFactory::createDecoder(CLCompressedAudioData* data)
{
	return new CLFFmpegAudioDecoder(data);
}
