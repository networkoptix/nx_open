#include "ffmpeg_audio.h"

#include "libavcodec/avcodec.h"
#include <QMutexLocker>
#include "base/log.h"
#include "../ffmpeg_dll/ffmpeg_dll.h"

#define INBUF_SIZE 4096

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

		
	case CL_AC3:
		codec = global_ffmpeg_dll.avcodec_find_decoder(CODEC_ID_AC3);
		break;

	case CL_WMAV2:
		codec = global_ffmpeg_dll.avcodec_find_decoder(CODEC_ID_WMAV2);
		break;
		/**/


	default:
		codec = 0;
		return;


	}



	c = global_ffmpeg_dll.avcodec_alloc_context();
	global_ffmpeg_dll.avcodec_open(c, codec);


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

	if (!codec)
		return false;

	const unsigned char* inbuf_ptr = data.inbuf;
	int size = data.inbuf_len;
	unsigned char* outbuf = data.outbuf;


	data.outbuf_len = 0;

	while (size > 0) 
	{

		int out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;


		cl_log.log("before dec",  cl_logALWAYS);

		int len = global_ffmpeg_dll.avcodec_decode_audio2(c, (short *)outbuf, &out_size,
				inbuf_ptr, size);


		cl_log.log("after dec",  cl_logALWAYS);

		if (len < 0) 
			return false;

		data.outbuf_len+=out_size;
		outbuf+=out_size;
		size -= len;
		inbuf_ptr += len;
	}


	data.format.setFrequency(c->sample_rate);
	data.format.setChannels(c->channels);
	data.format.setCodec("audio/pcm");
	data.format.setByteOrder(QAudioFormat::LittleEndian);


	switch(c->sample_fmt)
	{
	case SAMPLE_FMT_U8: ///< unsigned 8 bits
		data.format.setSampleSize(8);
		data.format.setSampleType(QAudioFormat::UnSignedInt);
		break;

	case SAMPLE_FMT_S16: ///< signed 16 bits
		data.format.setSampleSize(16);
		data.format.setSampleType(QAudioFormat::SignedInt);
		break;

	case SAMPLE_FMT_S32:///< signed 32 bits
		data.format.setSampleSize(32);
		data.format.setSampleType(QAudioFormat::SignedInt);
	    break;
	}

	return true;
}
