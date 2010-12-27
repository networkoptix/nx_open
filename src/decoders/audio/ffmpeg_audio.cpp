#include "ffmpeg_audio.h"

#include "libavcodec/avcodec.h"
#include <QMutexLocker>
#include "base/log.h"
#include "../ffmpeg_dll/ffmpeg_dll.h"



extern QMutex global_ffmpeg_mutex;
extern FFMPEGCodecDll global_ffmpeg_dll;


bool CLFFmpegAudioDecoder::m_first_instance = true;

//================================================
CLFFmpegAudioDecoder::CLFFmpegAudioDecoder(CLCodecType codec_id):
c(0),
m_codec(codec_id)
{

	QMutexLocker mutex(&global_ffmpeg_mutex);

	if (m_first_instance)
	{
		m_first_instance = false;

		// must be called before using avcodec 
		global_ffmpeg_dll.avcodec_init();

		// register all the codecs (you can also register only the codec you wish to have smaller code
		global_ffmpeg_dll.avcodec_register_all();

	}



	switch(m_codec)
	{
	case CL_MP3:
		codec = global_ffmpeg_dll.avcodec_find_decoder(CODEC_ID_MP3);
		break;

	
	default:
		codec = 0;


	}



	c = global_ffmpeg_dll.avcodec_alloc_context();


	

	global_ffmpeg_dll.avcodec_open(c, codec);

	c->debug_mv = 1;

}

CLFFmpegAudioDecoder::~CLFFmpegAudioDecoder(void)
{
	QMutexLocker mutex(&global_ffmpeg_mutex);

	global_ffmpeg_dll.avcodec_close(c);
	global_ffmpeg_dll.av_free(c);

}


//The input buffer must be FF_INPUT_BUFFER_PADDING_SIZE larger than the actual read bytes because some optimized bit stream readers read 32 or 64 bits at once and could read over the end.
//The end of the input buffer buf should be set to 0 to ensure that no over reading happens for damaged MPEG streams.
bool CLFFmpegAudioDecoder::decode(CLAudioData& data)
{
	return false;
}
