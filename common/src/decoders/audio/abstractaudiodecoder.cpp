#include "abstractaudiodecoder.h"
#include "ffmpeg_audio.h"


CLAbstractAudioDecoder* CLAudioDecoderFactory::createDecoder(QnCompressedAudioDataPtr data)
{
	return new CLFFmpegAudioDecoder(data);
}
