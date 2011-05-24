#include "ffmpeg.h"

#ifdef _USE_DXVA
#include "dxva/ffmpeg_callbacks.h"
#endif

extern QMutex global_ffmpeg_mutex;

#define LIGHT_CPU_MODE_FRAME_PERIOD 30
bool CLFFmpegVideoDecoder::m_first_instance = true;
int CLFFmpegVideoDecoder::hwcounter = 0;

//================================================

CLFFmpegVideoDecoder::CLFFmpegVideoDecoder(CodecID codec_id, AVCodecContext* codecContext):
m_passedContext(codecContext),
m_width(0),
m_height(0),
m_codecId(codec_id),
m_showmotion(false),
m_lightCPUMode(false),
m_wantEscapeFromLightCPUMode(false),
m_lightModeFrameCounter(0),
needResetCodec(false),
m_lastWidth(0)
{

    // XXX Debug, should be passed in constructor
    m_tryHardwareAcceleration = false; //hwcounter % 2;

	QMutexLocker mutex(&global_ffmpeg_mutex);

	if (m_first_instance)
	{
		m_first_instance = false;

		// must be called before using avcodec 
		avcodec_init();

		// register all the codecs (you can also register only the m_codec you wish to have smaller code
		avcodec_register_all();

        cl_log.log("FFMEG version = ", (int)avcodec_version(), cl_logALWAYS) ;

	}

	//m_codec = avcodec_find_decoder(CODEC_ID_H264);

    openDecoder();
}

AVCodec* CLFFmpegVideoDecoder::findCodec(CodecID codecId)
{
    AVCodec* codec = 0;

    // CodecID codecId = internalCodecIdToFfmpeg(internalCodecId);
    if (codecId != CODEC_ID_NONE)
        codec = avcodec_find_decoder(codecId);

    return codec;
}

void CLFFmpegVideoDecoder::closeDecoder()
{   
    avcodec_close(c);
#ifdef _USE_DXVA
    m_decoderContext.close();
#endif

    av_free(picture);
    av_free(c);
}

void CLFFmpegVideoDecoder::openDecoder()
{
    m_codec = findCodec(m_codecId);

    c = avcodec_alloc_context();

    if (m_passedContext) {
        avcodec_copy_context(c, m_passedContext);
    }

#ifdef _USE_DXVA
    if (m_codecId == CODEC_ID_H264)
    {
        c->get_format = FFMpegCallbacks::ffmpeg_GetFormat;
        c->get_buffer = FFMpegCallbacks::ffmpeg_GetFrameBuf;
        c->release_buffer = FFMpegCallbacks::ffmpeg_ReleaseFrameBuf;

        c->opaque = &m_decoderContext;
    }
#endif

    picture = avcodec_alloc_frame();

    //if(m_codec->capabilities&CODEC_CAP_TRUNCATED)	c->flags|= CODEC_FLAG_TRUNCATED;

    //c->debug_mv = 1;

    c->thread_count = QThread::idealThreadCount();
    c->thread_type = FF_THREAD_SLICE;

    // TODO: check return value
    if (avcodec_open(c, m_codec) < 0)
    {
        m_codec = 0;
    }
}

CLFFmpegVideoDecoder::~CLFFmpegVideoDecoder(void)
{
	QMutexLocker mutex(&global_ffmpeg_mutex);

    closeDecoder();
}

void CLFFmpegVideoDecoder::resetDecoder()
{
    QMutexLocker mutex(&global_ffmpeg_mutex);

    closeDecoder();
    openDecoder();
}
//The input buffer must be FF_INPUT_BUFFER_PADDING_SIZE larger than the actual read bytes because some optimized bitstream readers read 32 or 64 bits at once and could read over the end.
//The end of the input buffer buf should be set to 0 to ensure that no overreading happens for damaged MPEG streams.
bool CLFFmpegVideoDecoder::decode(CLVideoData& data)
{

	if (m_codec==0)
    {
        cl_log.log("decoder not found: m_codec = 0", cl_logWARNING);
		return false;
    }

	if (m_wantEscapeFromLightCPUMode && data.keyFrame)
	{
		m_wantEscapeFromLightCPUMode = false;
		m_lightModeFrameCounter = 0;
		m_lightCPUMode = false;
	}

	if (m_lightCPUMode)
	{

		if (data.codec == CODEC_ID_MJPEG)
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
			if (!data.keyFrame)
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

    // XXX: DEBUG

    bool needResetCodec = false;

    //if (m_lastWidth != 0 && m_lastWidth != data.width)
    //{
    //    needResetCodec = true;
    //}

#ifdef _USE_DXVA
    if (m_decoderContext.isHardwareAcceleration() && !isHardwareAccellerationPossible(data.codec, data.width, data.height))
    {
        m_decoderContext.setHardwareAcceleration(false);
        needResetCodec = true;
    }

    if (m_tryHardwareAcceleration && !m_decoderContext.isHardwareAcceleration() && isHardwareAccellerationPossible(data.codec, data.width, data.height))
    {
        m_decoderContext.setHardwareAcceleration(true);
        needResetCodec = true;
    }
#endif


    if(needResetCodec)
    {
        needResetCodec = false;
        resetDecoder();
    }

    AVPacket avpkt;
    av_init_packet(&avpkt);
    avpkt.data = (unsigned char*)data.inBuffer;
    avpkt.size = data.bufferLength;
    // HACK for CorePNG to decode as normal PNG by default
    avpkt.flags = AV_PKT_FLAG_KEY;
    
    int got_picture = 0;
    avcodec_decode_video2(c, picture, &got_picture, &avpkt);

	if (data.useTwice)
        avcodec_decode_video2(c, picture, &got_picture, &avpkt);

	if (got_picture )
	{
        m_lastWidth = data.width;
#ifdef _USE_DXVA
        if (m_decoderContext.isHardwareAcceleration())
        {
            m_decoderContext.extract(picture);
        }
#endif

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

		//if (m_showmotion)
		//	ff_print_debug_info((MpegEncContext*)(c->priv_data), picture);

		data.outFrame.width = c->width;
		data.outFrame.height = c->height;

		data.outFrame.C1 = picture->data[0];
		data.outFrame.C2 = picture->data[1];
		data.outFrame.C3 = picture->data[2];

		data.outFrame.stride1 = picture->linesize[0];
		data.outFrame.stride2 = picture->linesize[1];
		data.outFrame.stride3 = picture->linesize[2];

		switch (c->pix_fmt)
		{
		case PIX_FMT_YUVJ422P:
			data.outFrame.out_type = CL_DECODER_YUV422;
			break;
		case PIX_FMT_YUVJ444P:
			data.outFrame.out_type = CL_DECODER_YUV444;
			break;
		case PIX_FMT_YUV420P:
        case PIX_FMT_YUVJ420P:
			data.outFrame.out_type = CL_DECODER_YUV420;
			break;
		case PIX_FMT_UYVY422: //// must be romoved; just coz using all decoders witn new header
			data.outFrame.out_type = CL_DECODER_YUV444;
			break;

		case PIX_FMT_XVMC_MPEG2_IDCT: // must be romoved; just coz using all decoders witn new header
			data.outFrame.out_type = CL_DECODER_YUV422;
			break;

		case PIX_FMT_RGB555LE:
			data.outFrame.out_type = CL_DECODER_RGB555LE;
			break;

        case PIX_FMT_RGB24:
            data.outFrame.out_type = CL_DECODER_RGB24;
            break;

		default:
			//data.outFrame.out_type = CL_DECODER_YUV420;
            return false;
			break;
		}

		return true;
	}
	else
	{
		/*/			
		// some times decoder wants to delay frame by one; we do not want that
		avcodec_decode_video(c, picture, &got_picture,0, 0);
		if (got_picture)
			goto gotpicture;

		/**/

        cl_log.log("cannot decode image", cl_logWARNING);

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
