#include "ffmpegvideodecoder_p.h"

#include <QtCore/QThread>

#include "utils/ffmpeg/ffmpeg_global.h"
#ifdef _USE_DXVA
#  include "dxva/ffmpeg_dxva_callbacks_p.h"
#endif

#define LIGHT_CPU_MODE_FRAME_PERIOD 30

CLFFmpegVideoDecoder::CLFFmpegVideoDecoder(int codecId, AVCodecContext *codecContext)
    : m_codec(0), m_context(0), m_passedContext(0), m_frame(0),
#ifdef _USE_DXVA
    m_dxvaDecoderContext(0),
#endif
    m_width(0), m_height(0),
    m_codecId(codecId),
    m_showmotion(false),
    m_lastWidth(0)
{
    QnFFmpeg::initialize();

    if (codecContext)
    {
        m_passedContext = avcodec_alloc_context();
        avcodec_copy_context(m_passedContext, codecContext);
    }

    // ### Debug, should be passed in constructor
    m_tryHardwareAcceleration = false;

    createDecoder();
}

CLFFmpegVideoDecoder::~CLFFmpegVideoDecoder()
{
    destroyDecoder();

    if (m_passedContext)
        QnFFmpeg::closeCodec(m_passedContext);
}

bool CLFFmpegVideoDecoder::isCodecSupported(int codecId)
{
    Q_UNUSED(codecId)
    return true;
}

bool CLFFmpegVideoDecoder::isHardwareAccellerationPossible(int codecId, int width, int height)
{
    return codecId == CODEC_ID_H264 && width <= 1920 && height <= 1088;
}

void CLFFmpegVideoDecoder::resetDecoder()
{
    destroyDecoder();
    createDecoder();
}

void CLFFmpegVideoDecoder::createDecoder()
{
    Q_ASSERT(!m_codec);

    m_codec = QnFFmpeg::findDecoder(CodecID(m_codecId));
    if (!m_codec)
    {
        setError(CodecError);
        cl_log.log(QLatin1String("CLFFmpegVideoDecoder::createDecoder(): codec not found!"), cl_logWARNING);
        return;
    }

    m_context = avcodec_alloc_context();
    if (m_passedContext)
        avcodec_copy_context(m_context, m_passedContext);

#ifdef _USE_DXVA
    m_dxvaDecoderContext = new DxvaDecoderContext;
    if (m_codecId == CODEC_ID_H264)
    {
        m_context->get_format = FFmpegDXVACallbacks::ffmpeg_GetFormat;
        m_context->get_buffer = FFmpegDXVACallbacks::ffmpeg_GetFrameBuf;
        m_context->release_buffer = FFmpegDXVACallbacks::ffmpeg_ReleaseFrameBuf;

        m_context->opaque = m_dxvaDecoderContext;
    }
#endif

    m_frame = avcodec_alloc_frame();
    m_deinterlacedFrame = avcodec_alloc_frame();

    //if (m_codec->capabilities & CODEC_CAP_TRUNCATED) m_context->flags |= CODEC_FLAG_TRUNCATED;

    //m_context->debug_mv = 1;

    m_context->thread_count = qMin(QThread::idealThreadCount() + 1, 4);
    m_context->thread_type = m_mtDecoding ? FF_THREAD_FRAME : FF_THREAD_SLICE;
    cl_log.log(QLatin1String("Creating ") + QLatin1String(m_mtDecoding ? "FRAME threaded decoder" : "SLICE threaded decoder"), cl_logALWAYS);

    if (!QnFFmpeg::openCodec(m_context, m_codec))
    {
        destroyDecoder();
        setError(CodecContextError);
        cl_log.log(QLatin1String("CLFFmpegVideoDecoder::createDecoder(): could not open codec!"), cl_logWARNING);
        return;
    }

    const int numBytes = avpicture_get_size(PIX_FMT_YUV420P, m_context->width, m_context->height);
    m_deinterlaceBuffer = (uchar *)av_malloc(numBytes);

    avpicture_fill((AVPicture *)m_deinterlacedFrame, m_deinterlaceBuffer, PIX_FMT_YUV420P, m_context->width, m_context->height);
}

void CLFFmpegVideoDecoder::destroyDecoder()
{
    Q_ASSERT(m_context);

    QnFFmpeg::closeCodec(m_context);

#ifdef _USE_DXVA
    m_dxvaDecoderContext->close();
    delete m_dxvaDecoderContext;
    m_dxvaDecoderContext = 0;
#endif

    av_free(m_frame);
    m_frame = 0;
    av_free(m_deinterlaceBuffer);
    m_deinterlaceBuffer = 0;
    av_free(m_deinterlacedFrame);
    m_deinterlacedFrame = 0;
    av_free(m_context);
    m_context = 0;
}

//The input buffer must be FF_INPUT_BUFFER_PADDING_SIZE larger than
//the actual read bytes because some optimized bitstream readers
//read 32 or 64 bits at once and could read over the end.
//The end of the input buffer buf should be set to 0 to ensure
//that no overreading happens for damaged MPEG streams.
bool CLFFmpegVideoDecoder::decode(CLVideoData &data)
{
    if (!m_context)
        return false;

    /*if (width != m_width || height != m_height))
    {
        m_width = width;
        m_height = height;
    }*/

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

    */

    // XXX: DEBUG

    bool needResetCodec = m_needRecreate && data.keyFrame;

    //if (m_lastWidth != 0 && m_lastWidth != data.width)
    //{
    //    needResetCodec = true;
    //}

#ifdef _USE_DXVA
    if (m_dxvaDecoderContext->isHardwareAcceleration() && !isHardwareAccellerationPossible(data.codec, data.width, data.height))
    {
        m_dxvaDecoderContext->setHardwareAcceleration(false);
        needResetCodec = true;
    }
    else if (m_tryHardwareAcceleration && !m_dxvaDecoderContext->isHardwareAcceleration() && isHardwareAccellerationPossible(data.codec, data.width, data.height))
    {
        m_dxvaDecoderContext->setHardwareAcceleration(true);
        needResetCodec = true;
    }
#endif

    if (needResetCodec)
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
    avcodec_decode_video2(m_context, m_frame, &got_picture, &avpkt);

    if (data.useTwice)
        avcodec_decode_video2(m_context, m_frame, &got_picture, &avpkt);

    if (got_picture)
    {
        AVFrame *outputFrame = m_frame;
        if (m_frame->interlaced_frame && m_mtDecoding)
        {
            if (avpicture_deinterlace((AVPicture*)m_deinterlacedFrame, (AVPicture *)m_frame, m_context->pix_fmt, m_context->width, m_context->height) == 0)
                outputFrame = m_deinterlacedFrame;
        }

        m_lastWidth = data.width;
#ifdef _USE_DXVA
        if (m_dxvaDecoderContext->isHardwareAcceleration())
            m_dxvaDecoderContext->extract(m_frame);
#endif

        m_context->debug_mv = 0;
        if (m_showmotion)
        {
            m_context->debug_mv = 1;
            //m_context->debug |= FF_DEBUG_QP | FF_DEBUG_SKIP | FF_DEBUG_MB_TYPE | FF_DEBUG_VIS_QP | FF_DEBUG_VIS_MB_TYPE;
            //m_context->debug |= FF_DEBUG_VIS_MB_TYPE;
            //ff_print_debug_info((MpegEncContext*)(m_context->priv_data), picture);
        }

        data.outFrame.width = m_context->width;
        data.outFrame.height = m_context->height;

        data.outFrame.C1 = outputFrame->data[0];
        data.outFrame.C2 = outputFrame->data[1];
        data.outFrame.C3 = outputFrame->data[2];

        data.outFrame.stride1 = outputFrame->linesize[0];
        data.outFrame.stride2 = outputFrame->linesize[1];
        data.outFrame.stride3 = outputFrame->linesize[2];

        if (m_context->pix_fmt == PIX_FMT_NONE)
        {
            setError(FrameDecodeError);
            cl_log.log("CLFFmpegVideoDecoder::decode(): unsupported pixel format!", cl_logWARNING);
            return false;
        }

        // Filter deprecated pixel formats
        switch (m_context->pix_fmt)
        {
        case PIX_FMT_YUVJ420P:
            data.outFrame.out_type = PIX_FMT_YUV420P;
            break;

        case PIX_FMT_YUVJ422P:
            data.outFrame.out_type = PIX_FMT_YUV422P;
            break;

        case PIX_FMT_YUVJ444P:
            data.outFrame.out_type = PIX_FMT_YUV444P;
            break;

        default:
            data.outFrame.out_type = m_context->pix_fmt;
        }

        setError(NoError);

        return true;
    }

    /*
    // some times decoder wants to delay frame by one; we do not want that
    avcodec_decode_video(m_context, picture, &got_picture, 0, 0);
    if (got_picture)
        goto gotpicture;
    */

    setError(FrameDecodeError);
    //cl_log.log("CLFFmpegVideoDecoder::decode(): cannot decode image!", cl_logWARNING);

    return false;
}

void CLFFmpegVideoDecoder::showMotion(bool show)
{
    m_showmotion = show;
}
