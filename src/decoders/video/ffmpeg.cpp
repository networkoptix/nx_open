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
m_passedContext(0),
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
	if (codecContext)
	{
		m_passedContext = avcodec_alloc_context();
		avcodec_copy_context(m_passedContext, codecContext);
	}

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

		cl_log.log(QLatin1String("FFMEG version = "), (int)avcodec_version(), cl_logALWAYS) ;

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

	av_free(m_frame);
	av_free(m_deinterlaceBuffer);
	av_free(m_deinterlacedFrame);
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

	m_frame = avcodec_alloc_frame();
	m_deinterlacedFrame = avcodec_alloc_frame();

	//if(m_codec->capabilities&CODEC_CAP_TRUNCATED)	c->flags|= CODEC_FLAG_TRUNCATED;

	//c->debug_mv = 1;

    c->thread_count = qMin(4, QThread::idealThreadCount() + 1);
    c->thread_type = m_mtDecoding ? FF_THREAD_FRAME : FF_THREAD_SLICE;


    cl_log.log(QLatin1String("Creating ") + QLatin1String(m_mtDecoding ? "FRAME threaded decoder" : "SLICE threaded decoder"), cl_logALWAYS);
    // TODO: check return value
    if (avcodec_open(c, m_codec) < 0)
    {
        m_codec = 0;
    }

	int numBytes = avpicture_get_size(PIX_FMT_YUV420P, c->width, c->height);
	m_deinterlaceBuffer = (quint8*)av_malloc(numBytes * sizeof(quint8));
	avpicture_fill((AVPicture *)m_deinterlacedFrame, m_deinterlaceBuffer, PIX_FMT_YUV420P, c->width, c->height);

//	avpicture_fill((AVPicture *)picture, m_buffer, PIX_FMT_YUV420P, c->width, c->height);
}

CLFFmpegVideoDecoder::~CLFFmpegVideoDecoder(void)
{
    QMutexLocker mutex(&global_ffmpeg_mutex);

	closeDecoder();

	if (m_passedContext)
		avcodec_close(m_passedContext);
}

void CLFFmpegVideoDecoder::resetDecoder()
{
    QMutexLocker mutex(&global_ffmpeg_mutex);

    closeDecoder();
    openDecoder();
}
//The input buffer must be FF_INPUT_BUFFER_PADDING_SIZE larger than the actual read bytes because some optimized bitstream readers read 32 or 64 bits at once and could read over the end.
//The end of the input buffer buf should be set to 0 to ensure that no overreading happens for damaged MPEG streams.
bool CLFFmpegVideoDecoder::decode(const CLVideoData& data, CLVideoDecoderOutput* outFrame)
{


    if (m_codec==0)
    {
        cl_log.log(QLatin1String("decoder not found: m_codec = 0"), cl_logWARNING);
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

	bool needResetCodec = false;

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

    if (m_needRecreate && data.keyFrame)
    {
        m_needRecreate = false;
        resetDecoder();
    }

    AVPacket avpkt;
    av_init_packet(&avpkt);
    avpkt.data = (unsigned char*)data.inBuffer;
    avpkt.size = data.bufferLength;
    // HACK for CorePNG to decode as normal PNG by default
    avpkt.flags = AV_PKT_FLAG_KEY;

    int got_picture = 0;
    if (!outFrame->getUseExternalData() && 
        (outFrame->width != c->width || outFrame->height != c->height || outFrame->format != c->pix_fmt))
    {
        outFrame->clean();
        int numBytes = avpicture_get_size(c->pix_fmt, c->width, c->height);
        avpicture_fill((AVPicture*) outFrame, (quint8*) av_malloc(numBytes), c->pix_fmt, c->width, c->height);
        outFrame->width = c->width, 
        outFrame->height = c->height;
    }

    avcodec_decode_video2(c, m_frame, &got_picture, &avpkt);
    if (data.useTwice)
        avcodec_decode_video2(c, m_frame, &got_picture, &avpkt);

	if (got_picture )
	{
        AVFrame* copyFromFrame = m_frame;
		//AVFrame* outputFrame;
		if (m_frame->interlaced_frame && m_mtDecoding)
		{
            if (outFrame->getUseExternalData())
            {
			    if (avpicture_deinterlace((AVPicture*)m_deinterlacedFrame, (AVPicture*) m_frame, c->pix_fmt, c->width, c->height) == 0)
                    copyFromFrame = m_deinterlacedFrame;
            }
            else
                avpicture_deinterlace((AVPicture*) outFrame, (AVPicture*) m_frame, c->pix_fmt, c->width, c->height);
		}
        else {
            if (!outFrame->getUseExternalData())
                av_picture_copy((AVPicture*) outFrame, (AVPicture*) (m_frame), c->pix_fmt, c->width, c->height);
        }

        m_lastWidth = data.width;
#ifdef _USE_DXVA
        if (m_decoderContext.isHardwareAcceleration())
        {
            m_decoderContext.extract(m_frame);
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
        if (outFrame->getUseExternalData())
        {
		    outFrame->width = c->width;
		    outFrame->height = c->height;

		    outFrame->data[0] = copyFromFrame->data[0];
		    outFrame->data[1] = copyFromFrame->data[1];
		    outFrame->data[2] = copyFromFrame->data[2];

		    outFrame->linesize[0] = copyFromFrame->linesize[0];
		    outFrame->linesize[1] = copyFromFrame->linesize[1];
		    outFrame->linesize[2] = copyFromFrame->linesize[2];
        }
        outFrame->format = GetPixelFormat();
		return c->pix_fmt != PIX_FMT_NONE;
	}
    return false; // no picture decoded at current step
}

PixelFormat CLFFmpegVideoDecoder::GetPixelFormat()
{
    // Filter deprecated pixel formats
    switch(c->pix_fmt)
    {
    case PIX_FMT_YUVJ420P:
        return PIX_FMT_YUV420P;
    case PIX_FMT_YUVJ422P:
        return PIX_FMT_YUV422P;
    case PIX_FMT_YUVJ444P:
        return PIX_FMT_YUV444P;
    }
    return c->pix_fmt;
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
