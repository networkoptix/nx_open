#include "ffmpeg.h"


#include <QMutexLocker>
#include "..\ffmpeg_dll\ffmpeg_dll.h"


extern QMutex global_ffmpeg_mutex;
extern FFMPEGCodecDll global_ffmpeg_dll;


#define LIGHT_CPU_MODE_FRAME_PERIOD 30
bool CLFFmpegVideoDecoder::m_first_instance = true;


//================================================


CLFFmpegVideoDecoder::CLFFmpegVideoDecoder(CLCodecType codec_id, AVCodecContext* codecContext):
c(codecContext),
m_width(0),
m_height(0),
m_codec(codec_id),
m_showmotion(false),
m_lightCPUMode(false),
m_wantEscapeFromLightCPUMode(false),
m_lightModeFrameCounter(0)
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

	//codec = global_ffmpeg_dll.avcodec_find_decoder(CODEC_ID_H264);


	switch(m_codec)
	{
	case CL_JPEG:
		codec = global_ffmpeg_dll.avcodec_find_decoder(CODEC_ID_MJPEG);
		break;

	case CL_MPEG2:
		codec = global_ffmpeg_dll.avcodec_find_decoder(CODEC_ID_MPEG2VIDEO);
		break;


	case CL_MPEG4:
		codec = global_ffmpeg_dll.avcodec_find_decoder(CODEC_ID_MPEG4);
		break;

	case CL_MSMPEG4V2:
		codec = global_ffmpeg_dll.avcodec_find_decoder(CODEC_ID_MSMPEG4V2);
		break;


	case CL_MSMPEG4V3:
		codec = global_ffmpeg_dll.avcodec_find_decoder(CODEC_ID_MSMPEG4V3);
		break;


	case CL_H264:
		codec = global_ffmpeg_dll.avcodec_find_decoder(CODEC_ID_H264);
	    break;

	case CL_MSVIDEO1:
		codec = global_ffmpeg_dll.avcodec_find_decoder(CODEC_ID_MSVIDEO1);
		break;
	
	default:
		codec = 0;


	}

	c = global_ffmpeg_dll.avcodec_alloc_context();
	if (codecContext)	
	{
		c->extradata_size = codecContext->extradata_size;
		c->extradata = codecContext->extradata;
	}



	picture= global_ffmpeg_dll.avcodec_alloc_frame();

	//if(codec->capabilities&CODEC_CAP_TRUNCATED)	c->flags|= CODEC_FLAG_TRUNCATED;

	global_ffmpeg_dll.avcodec_open(c, codec);

	c->debug_mv = 1;

}

CLFFmpegVideoDecoder::~CLFFmpegVideoDecoder(void)
{
	QMutexLocker mutex(&global_ffmpeg_mutex);

	global_ffmpeg_dll.avcodec_close(c);
	global_ffmpeg_dll.av_free(c);
	global_ffmpeg_dll.av_free(picture);
}


//The input buffer must be FF_INPUT_BUFFER_PADDING_SIZE larger than the actual read bytes because some optimized bitstream readers read 32 or 64 bits at once and could read over the end.
//The end of the input buffer buf should be set to 0 to ensure that no overreading happens for damaged MPEG streams.
bool CLFFmpegVideoDecoder::decode(CLVideoData& data)
{

	if (m_wantEscapeFromLightCPUMode && data.key_frame)
	{
		m_wantEscapeFromLightCPUMode = false;
		m_lightModeFrameCounter = 0;
		m_lightCPUMode = false;
	}


	if (m_lightCPUMode)
	{

		if (data.codec == CL_JPEG)
		{
			if (m_lightModeFrameCounter < LIGHT_CPU_MODE_FRAME_PERIOD )
			{
				++m_lightModeFrameCounter;
				return false;
			}
			else
				m_lightModeFrameCounter = 0;


		}
		else // h.264
		{
			if (!data.key_frame)
				return false;
		}
	}

	
	/*
	if ((data.!=m_width)||(height!=m_height))
	{
		m_width = width;
		m_height = height;

	}
	/**/

	/*
	
	FILE * f = fopen("test.264_", "ab");
	fwrite(data.inbuf,1,data.buff_len,f);
	fclose(f);

	static int fn = 0;
	QString filename = "frame";
	filename+=QString::number(fn);
	filename+=".264";


	FILE * f2 = fopen(filename.toLatin1(), "wb");
	fwrite(data.inbuf,1,data.buff_len,f2);
	fclose(f2);
	fn++;

	/**/

	int got_picture = 0;
	global_ffmpeg_dll.avcodec_decode_video(c, picture, &got_picture,(unsigned char*)data.inbuf, data.buff_len);
	if (data.use_twice)
		global_ffmpeg_dll.avcodec_decode_video(c, picture, &got_picture,(unsigned char*)data.inbuf, data.buff_len);


gotpicture:

	if (got_picture )
	{
		if (m_showmotion)
		{
			c->debug_mv = 1;
			//c->debug |=  FF_DEBUG_QP | FF_DEBUG_SKIP | FF_DEBUG_MB_TYPE | FF_DEBUG_VIS_QP | FF_DEBUG_VIS_MB_TYPE;
			//c->debug |=  FF_DEBUG_VIS_MB_TYPE;
		}
		else
		{
			c->debug_mv = 0;
		}


		if (m_showmotion)
			global_ffmpeg_dll.ff_print_debug_info((MpegEncContext*)(c->priv_data), picture);

		data.out_frame.width = c->width;
		data.out_frame.height = c->height;

		data.out_frame.C1 = picture->data[0];
		data.out_frame.C2 = picture->data[1];
		data.out_frame.C3 = picture->data[2];

		data.out_frame.stride1 = picture->linesize[0];
		data.out_frame.stride2 = picture->linesize[1];
		data.out_frame.stride3 = picture->linesize[2];


		switch (c->pix_fmt)
		{
		case PIX_FMT_YUVJ422P:
			data.out_frame.out_type = CL_DECODER_YUV422;
			break;
		case PIX_FMT_YUVJ444P:
			data.out_frame.out_type = CL_DECODER_YUV444;
			break;
		case PIX_FMT_YUV420P:
			data.out_frame.out_type = CL_DECODER_YUV420;
			break;
		case PIX_FMT_UYVY422: //// must be romoved; just coz using all decoders witn new header
			data.out_frame.out_type = CL_DECODER_YUV444;
			break;


		case PIX_FMT_XVMC_MPEG2_IDCT: // must be romoved; just coz using all decoders witn new header
			data.out_frame.out_type = CL_DECODER_YUV422;
			break;



		default:
			data.out_frame.out_type = CL_DECODER_YUV420;
			break;
		}

		return true;

	}
	else
	{
		/*/			
		// some times decoder wants to delay frame by one; we do not want that
		global_ffmpeg_dll.avcodec_decode_video(c, picture, &got_picture,0, 0);
		if (got_picture)
			goto gotpicture;

		/**/


		return false;
	}



}

void CLFFmpegVideoDecoder::setLightCpuMode(bool val)
{
	if (val)
	{
		m_lightCPUMode = true;
		m_wantEscapeFromLightCPUMode = false;
		m_lightModeFrameCounter = 0;
	}
	else
	{
		m_wantEscapeFromLightCPUMode = true;
	}
}

void CLFFmpegVideoDecoder::showMotion(bool show)
{
		m_showmotion = show;
}