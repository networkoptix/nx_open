#include "ffmpeg.h"

#ifdef _USE_DXVA
#include "dxva/ffmpeg_callbacks.h"
#endif
#include "base/ffmpeg_helper.h"
#include "base/nalUnits.h"

extern QMutex global_ffmpeg_mutex;

static const int  LIGHT_CPU_MODE_FRAME_PERIOD = 15;
bool CLFFmpegVideoDecoder::m_first_instance = true;
int CLFFmpegVideoDecoder::hwcounter = 0;
static const quint32 LONG_NAL_PREFIX = htonl(1);

//================================================

CLFFmpegVideoDecoder::CLFFmpegVideoDecoder(CodecID codec_id, AVCodecContext* codecContext):
m_passedContext(0),
m_width(0),
m_height(0),
m_codecId(codec_id),
m_showmotion(false),
m_decodeMode(DecodeMode_Full),
m_newDecodeMode(DecodeMode_NotDefined),
m_lightModeFrameCounter(0),
m_frameTypeExtractor(0),
m_deinterlaceBuffer(0),
m_usedQtImage(false),
m_currentWidth(-1),
m_currentHeight(-1),
m_checkH264ResolutionChange(false)
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
void CLFFmpegVideoDecoder::flush()
{
    //avcodec_flush_buffers(c); // does not flushing output frames
    int got_picture = 0;
    AVPacket avpkt;
    avpkt.data = 0;
    avpkt.size = 0;
    while (avcodec_decode_video2(m_context, m_frame, &got_picture, &avpkt) > 0);
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
    avcodec_close(m_context);
#ifdef _USE_DXVA
    m_decoderContext.close();
#endif

	av_free(m_frame);
    if (m_deinterlaceBuffer)
	    av_free(m_deinterlaceBuffer);
	av_free(m_deinterlacedFrame);
	av_free(m_context);
    delete m_frameTypeExtractor;
}

void CLFFmpegVideoDecoder::openDecoder()
{
    m_codec = findCodec(m_codecId);

    m_context = avcodec_alloc_context();

    if (m_passedContext) {
        avcodec_copy_context(m_context, m_passedContext);
    }

    m_frameTypeExtractor = new FrameTypeExtractor(m_context);

#ifdef _USE_DXVA
    if (m_codecId == CODEC_ID_H264)
    {
        m_context->get_format = FFMpegCallbacks::ffmpeg_GetFormat;
        m_context->get_buffer = FFMpegCallbacks::ffmpeg_GetFrameBuf;
        m_context->release_buffer = FFMpegCallbacks::ffmpeg_ReleaseFrameBuf;

        m_context->opaque = &m_decoderContext;
    }
#endif

	m_frame = avcodec_alloc_frame();
	m_deinterlacedFrame = avcodec_alloc_frame();

	//if(m_codec->capabilities&CODEC_CAP_TRUNCATED)	c->flags|= CODEC_FLAG_TRUNCATED;

	//c->debug_mv = 1;

    m_context->thread_count = qMin(5, QThread::idealThreadCount() + 1);
    m_context->thread_type = m_mtDecoding ? FF_THREAD_FRAME : FF_THREAD_SLICE;
    m_checkH264ResolutionChange = m_mtDecoding && m_context->codec_id == CODEC_ID_H264 && m_context->extradata_size && m_context->extradata[0] == 0; 


    cl_log.log(QLatin1String("Creating ") + QLatin1String(m_mtDecoding ? "FRAME threaded decoder" : "SLICE threaded decoder"), cl_logALWAYS);
    // TODO: check return value
    if (avcodec_open(m_context, m_codec) < 0)
    {
        m_codec = 0;
    }

	int numBytes = avpicture_get_size(PIX_FMT_YUV420P, m_context->width, m_context->height);
    if (numBytes > 0) {
	    m_deinterlaceBuffer = (quint8*)av_malloc(numBytes * sizeof(quint8));
	    avpicture_fill((AVPicture *)m_deinterlacedFrame, m_deinterlaceBuffer, PIX_FMT_YUV420P, m_context->width, m_context->height);
    }

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

    //closeDecoder();
    //openDecoder();
    //return;

    // I have improved resetDecoder speed (I have left only minimum operations) because of REW. REW calls reset decoder on each GOP.
    avcodec_close(m_context);
    if (m_passedContext) 
        avcodec_copy_context(m_context, m_passedContext);
    m_context->thread_count = qMin(5, QThread::idealThreadCount() + 1);
    m_context->thread_type = m_mtDecoding ? FF_THREAD_FRAME : FF_THREAD_SLICE;
    // ensure that it is H.264 with nal prefixes
    m_checkH264ResolutionChange = m_mtDecoding && m_context->codec_id == CODEC_ID_H264 && m_context->extradata_size && m_context->extradata[0] == 0; 
    avcodec_open(m_context, m_codec);
}
//The input buffer must be FF_INPUT_BUFFER_PADDING_SIZE larger than the actual read bytes because some optimized bitstream readers read 32 or 64 bits at once and could read over the end.
//The end of the input buffer buf should be set to 0 to ensure that no overreading happens for damaged MPEG streams.
bool CLFFmpegVideoDecoder::decode(const CLCompressedVideoData& data, CLVideoDecoderOutput* outFrame)
{


    if (m_codec==0)
    {
        cl_log.log(QLatin1String("decoder not found: m_codec = 0"), cl_logWARNING);
        return false;
    }

	if (m_newDecodeMode != DecodeMode_NotDefined && (data.flags & AV_PKT_FLAG_KEY))
	{
        m_decodeMode = m_newDecodeMode;
		m_newDecodeMode = DecodeMode_NotDefined;
		m_lightModeFrameCounter = 0;
	}

	if (m_decodeMode > DecodeMode_Full && data.data.data())
	{

		if (data.compressionType == CODEC_ID_MJPEG)
		{
            int period = DecodeMode_Fast ? LIGHT_CPU_MODE_FRAME_PERIOD : LIGHT_CPU_MODE_FRAME_PERIOD*2;
			if (m_lightModeFrameCounter < period)
			{
				++m_lightModeFrameCounter;
				return false;
			}
			else
				m_lightModeFrameCounter = 0;

		}
		else if (!(data.flags & AV_PKT_FLAG_KEY))
		{
			if (m_decodeMode == DecodeMode_Fastest)
				return false;
            else if (m_frameTypeExtractor->getFrameType((quint8*) data.data.data(), data.data.size()) == FrameTypeExtractor::B_Frame)
                return false;
		}
	}

#ifdef _USE_DXVA
    bool needResetCodec = false;

    if (m_decoderContext.isHardwareAcceleration() && !isHardwareAccellerationPossible(data.codec, data.width, data.height)
        || m_tryHardwareAcceleration && !m_decoderContext.isHardwareAcceleration() && isHardwareAccellerationPossible(data.codec, data.width, data.height))
    {
        m_decoderContext.setHardwareAcceleration(!m_decoderContext.isHardwareAcceleration());
        m_needRecreate = true;
    }
#endif

    if (m_needRecreate && (data.flags & AV_PKT_FLAG_KEY))
    {
        m_needRecreate = false;
        resetDecoder();
    }

    AVPacket avpkt;
    av_init_packet(&avpkt);
    avpkt.data = (unsigned char*)data.data.data();
    avpkt.size = data.data.size();
    avpkt.dts = avpkt.pts = data.timestamp;
    // HACK for CorePNG to decode as normal PNG by default
    avpkt.flags = AV_PKT_FLAG_KEY;

    // from avcodec_decode_video2() docs:
    // 1) The input buffer must be FF_INPUT_BUFFER_PADDING_SIZE larger than
    // the actual read bytes because some optimized bitstream readers read 32 or 64
    // bits at once and could read over the end.
    // 2) The end of the input buffer buf should be set to 0 to ensure that
    // no overreading happens for damaged MPEG streams.

    // 1 is already guaranteed by CLByteArray, let's apply 2 here...
    if (avpkt.data)
        memset(avpkt.data + avpkt.size, 0, FF_INPUT_BUFFER_PADDING_SIZE);

    int got_picture = 0;

    // ### handle errors
    if (m_context->pix_fmt == -1)
        m_context->pix_fmt = PixelFormat(0);

    // workaround ffmpeg crush
    if (m_checkH264ResolutionChange && avpkt.size > 4 && *((quint32*)avpkt.data) == LONG_NAL_PREFIX && (avpkt.data[4]&0x1f) == nuSPS)
    {
        SPSUnit sps;
        const quint8* end = NALUnit::findNALWithStartCode(avpkt.data+4, avpkt.data + avpkt.size, true);
        sps.decodeBuffer(avpkt.data, end);
        sps.deserialize();
        if (m_currentWidth == -1) {
            m_currentWidth = sps.getWidth();
            m_currentHeight = sps.getHeight();
        }
        else if (sps.getWidth() != m_currentWidth || sps.getHeight() != m_currentHeight)
        {
            m_currentWidth = sps.getWidth();
            m_currentHeight = sps.getHeight();
            resetDecoder();
        }
    }
    // -------------------------

    avcodec_decode_video2(m_context, m_frame, &got_picture, &avpkt);
    if (data.useTwice)
        avcodec_decode_video2(m_context, m_frame, &got_picture, &avpkt);

    AVFrame* copyFromFrame = m_frame;

    // sometimes ffmpeg can't decode JPEG files. Try to decode in QT
    m_usedQtImage = false;
    if (m_context->codec_id == CODEC_ID_MJPEG && !got_picture) {
        m_tmpImg.loadFromData(avpkt.data, avpkt.size);
        if (m_tmpImg.width() > 0 && m_tmpImg.height() > 0) {
            got_picture = 1;
            m_tmpQtFrame.setUseExternalData(true);
            m_tmpQtFrame.format = PIX_FMT_BGRA;
            m_tmpQtFrame.data[0] = (quint8*) m_tmpImg.constBits();
            m_tmpQtFrame.data[1] = m_tmpQtFrame.data[2] = m_tmpQtFrame.data[3] = 0;
            m_tmpQtFrame.linesize[0] = m_tmpImg.bytesPerLine();
            m_tmpQtFrame.linesize[1] = m_tmpQtFrame.linesize[2] = m_tmpQtFrame.linesize[3] = 0;
            m_tmpQtFrame.width = m_tmpImg.width();
            m_tmpQtFrame.height = m_tmpImg.height();
            m_tmpQtFrame.pkt_dts = data.timestamp;
            copyFromFrame = &m_tmpQtFrame;
            m_usedQtImage = true;
        }
    }

    if (got_picture)
    {
        if (!outFrame->isExternalData() &&
            (outFrame->width != m_context->width || outFrame->height != m_context->height || 
            outFrame->format != m_context->pix_fmt || outFrame->linesize[0] != m_frame->linesize[0]))
        {
            outFrame->reallocate(m_context->width, m_context->height, m_context->pix_fmt, m_frame->linesize[0]);
        }

		if (m_frame->interlaced_frame && m_mtDecoding)
		{
            if (outFrame->isExternalData())
            {
                if (avpicture_deinterlace((AVPicture*)m_deinterlacedFrame, (AVPicture*) m_frame, m_context->pix_fmt, m_context->width, m_context->height) == 0) {
                    m_deinterlacedFrame->pkt_dts = m_frame->pkt_dts;
                    copyFromFrame = m_deinterlacedFrame;
                }
            }
            else {
                avpicture_deinterlace((AVPicture*) outFrame, (AVPicture*) m_frame, m_context->pix_fmt, m_context->width, m_context->height);
                outFrame->pkt_dts = m_frame->pkt_dts;
            }
		}
        else {
            if (!outFrame->isExternalData()) 
            {
                if (outFrame->format == PIX_FMT_YUV420P) 
                {
                    // optimization
                    for (int i = 0; i < 3; ++ i) 
                    {
                        int h = m_frame->height >> (i > 0 ? 1 : 0);
                        memcpy(outFrame->data[i], m_frame->data[i], m_frame->linesize[i]* h);
                    }
                }
                else {
                    av_picture_copy((AVPicture*) outFrame, (AVPicture*) (m_frame), m_context->pix_fmt, m_context->width, m_context->height);
                }

                outFrame->pkt_dts = m_frame->pkt_dts;
            }
        }

#ifdef _USE_DXVA
        if (m_decoderContext.isHardwareAcceleration())
            m_decoderContext.extract(m_frame);
#endif

		m_context->debug_mv = 0;
		if (m_showmotion)
		{
			m_context->debug_mv = 1;
			//c->debug |=  FF_DEBUG_QP | FF_DEBUG_SKIP | FF_DEBUG_MB_TYPE | FF_DEBUG_VIS_QP | FF_DEBUG_VIS_MB_TYPE;
			//c->debug |=  FF_DEBUG_VIS_MB_TYPE;
			//ff_print_debug_info((MpegEncContext*)(c->priv_data), picture);
		}

        if (outFrame->isExternalData())
        {
		    outFrame->width = m_context->width;
		    outFrame->height = m_context->height;

		    outFrame->data[0] = copyFromFrame->data[0];
		    outFrame->data[1] = copyFromFrame->data[1];
		    outFrame->data[2] = copyFromFrame->data[2];

		    outFrame->linesize[0] = copyFromFrame->linesize[0];
		    outFrame->linesize[1] = copyFromFrame->linesize[1];
		    outFrame->linesize[2] = copyFromFrame->linesize[2];
            outFrame->pkt_dts = copyFromFrame->pkt_dts;
        }
        outFrame->format = GetPixelFormat();
		return m_context->pix_fmt != PIX_FMT_NONE;
	}
    return false; // no picture decoded at current step
}

double CLFFmpegVideoDecoder::getSampleAspectRatio() const
{
    if (!m_context)
        return 1.0;

    double result = av_q2d(m_context->sample_aspect_ratio);

    if (qAbs(result)< 1e-7)
        result = 1.0; // if sample_aspect_ratio==0 it's unknown based on ffmpeg docs. so we assume it's 1.0 then

    return result;
}

PixelFormat CLFFmpegVideoDecoder::GetPixelFormat()
{
    if (m_usedQtImage)
        return PIX_FMT_BGRA;
    // Filter deprecated pixel formats
    switch(m_context->pix_fmt)
    {
    case PIX_FMT_YUVJ420P:
        return PIX_FMT_YUV420P;
    case PIX_FMT_YUVJ422P:
        return PIX_FMT_YUV422P;
    case PIX_FMT_YUVJ444P:
        return PIX_FMT_YUV444P;
    }
    return m_context->pix_fmt;
}

void CLFFmpegVideoDecoder::setLightCpuMode(CLAbstractVideoDecoder::DecodeMode val)
{
    //val = DecodeMode_Fast; // todo: debug only. remove me!!!
	if (val >= m_decodeMode)
	{
        m_decodeMode = val;
		m_newDecodeMode = DecodeMode_NotDefined;
		m_lightModeFrameCounter = 0;
	}
	else
	{
		m_newDecodeMode = val;
	}
}

void CLFFmpegVideoDecoder::showMotion(bool show)
{
		m_showmotion = show;
}
