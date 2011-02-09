#include "ffmpeg_audio.h"

#include "base/log.h"
#include "base/aligned_data.h"

struct AVCodecContext;

#define INBUF_SIZE 4096

extern QMutex global_ffmpeg_mutex;
extern int MAX_AUDIO_FRAME_SIZE;

bool CLFFmpegAudioDecoder::m_first_instance = true;

//================================================
CLFFmpegAudioDecoder::CLFFmpegAudioDecoder(CLCodecType codec_id, AVCodecContext* codecContext):
c(0),
m_codec(codec_id)
{

	QMutexLocker mutex(&global_ffmpeg_mutex);

	if (m_first_instance)
	{
		m_first_instance = false;

		// must be called before using avcodec 
		avcodec_init();

		// register all the codecs (you can also register only the codec you wish to have smaller code
		avcodec_register_all();

	}



	switch(m_codec)
	{

	case CL_MP2:
		codec = avcodec_find_decoder(CODEC_ID_MP2);
		break;

	case CL_MP3:
		codec = avcodec_find_decoder(CODEC_ID_MP3);
		break;

		
	case CL_AC3:
		codec = avcodec_find_decoder(CODEC_ID_AC3);
		break;

	case CL_AAC:
		codec = avcodec_find_decoder(CODEC_ID_AAC);
		break;


	case CL_WMAPRO:
		codec = avcodec_find_decoder(CODEC_ID_WMAPRO);
		break;

	case CL_WMAV2:
		codec = avcodec_find_decoder(CODEC_ID_WMAV2);
		break;

		/*/
		

	case CL_ADPCM_MS:
		codec = avcodec_find_decoder(CODEC_ID_ADPCM_MS);
		break;

		/**/


	default:
		codec = 0;
		c = 0;
		return;


	}

	c = avcodec_alloc_context();

	if (codecContext) {
		avcodec_copy_context(c, codecContext);
	}
	avcodec_open(c, codec);


}

CLFFmpegAudioDecoder::~CLFFmpegAudioDecoder(void)
{
	QMutexLocker mutex(&global_ffmpeg_mutex);

	if (c)
	{
		avcodec_close(c);
		av_free(c);
	}

}


//The input buffer must be FF_INPUT_BUFFER_PADDING_SIZE larger than the actual read bytes because some optimized bit stream readers read 32 or 64 bits at once and could read over the end.
//The end of the input buffer buf should be set to 0 to ensure that no over reading happens for damaged MPEG streams.
bool CLFFmpegAudioDecoder::decode(CLAudioData& data)
{

	if (!codec)
		return false;

	const unsigned char* inbuf_ptr = data.inbuf;
	int size = data.inbuf_len;
	unsigned char* outbuf = (unsigned char*)data.outbuf->data();
	
	

	data.outbuf_len = 0;

	while (size > 0) 
	{

		int out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;


		//cl_log.log("before dec",  cl_logALWAYS);

		if (data.outbuf_len + out_size > data.outbuf->capacity())
		{
			if (data.outbuf->capacity() >= MAX_AUDIO_FRAME_SIZE) // can not increase capacity any more
			{
				Q_ASSERT(false);
				data.outbuf_len = 0;
				outbuf = (unsigned char*)data.outbuf->data(); // start form beginning 
			}
			else
			{
				unsigned long new_capacity = qMin((unsigned long)MAX_AUDIO_FRAME_SIZE, (unsigned long)(data.outbuf->capacity()*1.5));
				new_capacity = qMax(new_capacity, data.outbuf_len + out_size);
				data.outbuf->change_capacity(new_capacity);
				outbuf = (unsigned char*)data.outbuf->data() + data.outbuf_len;

			}
		}


		int len = avcodec_decode_audio2(c, (short *)outbuf, &out_size,
				inbuf_ptr, size);


		//cl_log.log("after dec",  cl_logALWAYS);

		if (len < 0) 
			return false;


		data.outbuf_len+=out_size;
		outbuf+=out_size;
		size -= len;
		inbuf_ptr += len;

		
	}


	if (c->sample_rate)
		data.format.setFrequency(c->sample_rate);

	if (c->channels) 
		data.format.setChannels(c->channels);

	data.format.setCodec("audio/pcm");
	data.format.setByteOrder(QAudioFormat::BigEndian);

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

	case AV_SAMPLE_FMT_FLT:
		data.format.setSampleSize(32);
		data.format.setSampleType(QAudioFormat::Float);
		break;

	}


	return true;
}
