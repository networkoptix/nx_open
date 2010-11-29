#include "ffmpeg.h"

#include "libavcodec/avcodec.h"
#include <tchar.h>
#include <QMutexLocker>
#include <QDir>
#include "base/log.h"

#define LIGHT_CPU_MODE_FRAME_PERIOD 30

CLFFmpegVideoDecoder::CodecDll CLFFmpegVideoDecoder::dll;
bool CLFFmpegVideoDecoder::m_first_instance = true;
QMutex CLFFmpegVideoDecoder::m_static_mutex;



void decoderLogCallback(void* pParam, int i, const char* szFmt, va_list args)
{
	//USES_CONVERSION;

	//Ignore debug and info (i == 2 || i == 1) messages
	if(AV_LOG_ERROR != i)
	{
		//return;
	}

	AVCodecContext* pCtxt = (AVCodecContext*)pParam;



	char szMsg[1024];
	vsprintf(szMsg, szFmt, args);
	//if(szMsg[strlen(szMsg)] == '\n')
	{	
		szMsg[strlen(szMsg)-1] = 0;
	}

	cl_log.log("FFMPEG ", szMsg, cl_logERROR);
}

CLFFmpegVideoDecoder::CodecDll::CodecDll()
{

}
bool CLFFmpegVideoDecoder::CodecDll::init()
{

	QDir::setCurrent("./old_ffmpeg");

	//m_dll  = ::LoadLibrary(L"avcodec-52.dll");
	m_dll  = ::LoadLibrary(L"avcodec-51.dll");

	if(!m_dll)
		return false;

	avcodec_init = reinterpret_cast<dll_avcodec_init>(::GetProcAddress(m_dll, "avcodec_init"));
	if(!avcodec_init)
		return false;

	avcodec_find_decoder = reinterpret_cast<dll_avcodec_find_decoder>(::GetProcAddress(m_dll, "avcodec_find_decoder"));
	if(!avcodec_find_decoder)
		return false;

	avcodec_register_all = reinterpret_cast<dll_avcodec_register_all>(::GetProcAddress(m_dll, "avcodec_register_all"));
	if (!avcodec_register_all)
		return false;


	avcodec_alloc_context  = reinterpret_cast<dll_avcodec_alloc_context>(::GetProcAddress(m_dll, "avcodec_alloc_context"));
	if (!avcodec_register_all)
		return false;

	avcodec_alloc_frame  = reinterpret_cast<dll_avcodec_alloc_frame>(::GetProcAddress(m_dll, "avcodec_alloc_frame"));
	if (!avcodec_alloc_frame)
		return false;


	avcodec_open  = reinterpret_cast<dll_avcodec_open>(::GetProcAddress(m_dll, "avcodec_open"));
	if (!avcodec_open)
		return false;


	avcodec_close  = reinterpret_cast<dll_avcodec_close>(::GetProcAddress(m_dll, "avcodec_close"));
	if (!avcodec_close)
		return false;

	ff_print_debug_info = reinterpret_cast<dll_ff_print_debug_info>(::GetProcAddress(m_dll, "ff_print_debug_info"));
	if (!ff_print_debug_info)
		return false;
	
	//m_dll2 = ::LoadLibrary(L"avutil-50.dll");
	m_dll2 = ::LoadLibrary(L"avutil-49.dll");
	if(!m_dll2)
		return false;


	av_free = reinterpret_cast<dll_av_free>(::GetProcAddress(m_dll2, "av_free"));
	if (!av_free)
		return false;

	av_log_set_callback = reinterpret_cast<dll_av_log_set_callback>(::GetProcAddress(m_dll2, "av_log_set_callback"));
	if (!av_log_set_callback)
		return false;
	else
	{
		av_log_set_callback(decoderLogCallback);
	}



	avcodec_decode_video = reinterpret_cast<dll_avcodec_decode_video>(::GetProcAddress(m_dll, "avcodec_decode_video"));
	if (!avcodec_decode_video)
		return false;
	//==================================================================================

	QDir::setCurrent("../");


	return true;

}
CLFFmpegVideoDecoder::CodecDll::~CodecDll()
{
	::FreeLibrary(m_dll);
	::FreeLibrary(m_dll2);
}

//================================================


CLFFmpegVideoDecoder::CLFFmpegVideoDecoder(CLCodecType codec_id):
c(0),
m_width(0),
m_height(0),
m_codec(codec_id),
m_showmotion(false),
m_lightCPUMode(false),
m_wantEscapeFromLightCPUMode(false),
m_lightModeFrameCounter(0)
{

	QMutexLocker mutex(&m_static_mutex);

	if (m_first_instance)
	{
		m_first_instance = false;

		// must be called before using avcodec 
		dll.avcodec_init();

		// register all the codecs (you can also register only the codec you wish to have smaller code
		dll.avcodec_register_all();

	}

	//codec = dll.avcodec_find_decoder(CODEC_ID_H264);


	switch(m_codec)
	{
	case CL_JPEG:
		codec = dll.avcodec_find_decoder(CODEC_ID_MJPEG);
		break;

	case CL_MPEG2:
		codec = dll.avcodec_find_decoder(CODEC_ID_MPEG2VIDEO);
		break;


	case CL_MPEG4:
		codec = dll.avcodec_find_decoder(CODEC_ID_MPEG4);
		break;

	case CL_H264:
		codec = dll.avcodec_find_decoder(CODEC_ID_H264);
	    break;

	
	default:
		codec = 0;


	}



	c = dll.avcodec_alloc_context();
	picture= dll.avcodec_alloc_frame();

	//if(codec->capabilities&CODEC_CAP_TRUNCATED)	c->flags|= CODEC_FLAG_TRUNCATED;

	dll.avcodec_open(c, codec);

	c->debug_mv = 1;

}

CLFFmpegVideoDecoder::~CLFFmpegVideoDecoder(void)
{
	QMutexLocker mutex(&m_static_mutex);

	dll.avcodec_close(c);
	dll.av_free(c);
	dll.av_free(picture);
}

void CLFFmpegVideoDecoder::resart_decoder()
{
	QMutexLocker mutex(&m_static_mutex);

	if (c)
	{
		dll.avcodec_close(c);
		dll.av_free(c);
		dll.av_free(picture);
	}

	c = dll.avcodec_alloc_context();
	picture= dll.avcodec_alloc_frame();

	//if(codec->capabilities&CODEC_CAP_TRUNCATED)		c->flags|= CODEC_FLAG_TRUNCATED;

	dll.avcodec_open(c, codec);

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
		resart_decoder();
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
	dll.avcodec_decode_video(c, picture, &got_picture,(unsigned char*)data.inbuf, data.buff_len);

 //gotpicture:

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
			dll.ff_print_debug_info((MpegEncContext*)(c->priv_data), picture);

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

		default:
			data.out_frame.out_type = CL_DECODER_YUV420;
			break;
		}

		return true;

	}
	else
	{
		/*
		// some times decoder wants to delay frame by one; we do not want that
		dll.avcodec_decode_video(c, picture, &got_picture,0, 0);
		if (got_picture)
			goto gotpicture;
		else
			got_picture = got_picture;

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