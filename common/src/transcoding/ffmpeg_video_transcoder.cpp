#include "ffmpeg_video_transcoder.h"
#include "decoders/video/ffmpeg.h"
#include <utils/color_space/yuvconvert.h>

const static int MAX_VIDEO_FRAME = 1024 * 1024 * 3;
static const int TEXT_HEIGHT_IN_FRAME_PARTS = 25;
static const int MIN_TEXT_HEIGHT = 14;

QnFfmpegVideoTranscoder::QnFfmpegVideoTranscoder(CodecID codecId):
QnVideoTranscoder(codecId),
m_videoDecoder(0),
m_decodedVideoFrame(new CLVideoDecoderOutput()),
m_encoderCtx(0),
scaleContext(0),
m_firstEncodedPts(AV_NOPTS_VALUE),
m_lastSrcWidth(-1),
m_lastSrcHeight(-1),
m_mtMode(false),
m_dateTextPos(Date_None),
m_imageBuffer(0),
m_timeImg(0),
m_dateTimeXOffs(0),
m_dateTimeYOffs(0),
m_quality(QnQualityNormal),
m_onscreenDateOffset(0),
m_bufXOffs(0),
m_bufYOffs(0)
{
    m_videoEncodingBuffer = (quint8*) qMallocAligned(MAX_VIDEO_FRAME, 32);
}

QnFfmpegVideoTranscoder::~QnFfmpegVideoTranscoder()
{
    qFreeAligned(m_videoEncodingBuffer);
    if (scaleContext)
        sws_freeContext(scaleContext);

    if (m_encoderCtx) {
        avcodec_close(m_encoderCtx);
        av_free(m_encoderCtx);
    }

    delete m_videoDecoder;
    delete m_timeImg;
    qFreeAligned(m_imageBuffer);
}

int QnFfmpegVideoTranscoder::rescaleFrame()
{
    if (m_decodedVideoFrame->width != m_lastSrcWidth ||  m_decodedVideoFrame->height != m_lastSrcHeight)
    {
        // src resolution is changed
        m_lastSrcWidth = m_decodedVideoFrame->width;
        m_lastSrcHeight = m_decodedVideoFrame->height;
        if (scaleContext) {
            sws_freeContext(scaleContext);
            scaleContext = 0;
        }
    }

    if (scaleContext == 0)
    {
        scaleContext = sws_getContext(m_decodedVideoFrame->width, m_decodedVideoFrame->height, (PixelFormat) m_decodedVideoFrame->format, 
                                      m_resolution.width(), m_resolution.height(), (PixelFormat) PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);
        if (!scaleContext) {
            m_lastErrMessage = QObject::tr("Can't allocate scaler context for resolution %1x%2").arg(m_resolution.width()).arg(m_resolution.height());
            return -4;
        }
        m_scaledVideoFrame.reallocate(m_resolution.width(), m_resolution.height(), PIX_FMT_YUV420P);
    }

    sws_scale(scaleContext,
        m_decodedVideoFrame->data, m_decodedVideoFrame->linesize, 
        0, m_decodedVideoFrame->height, 
        m_scaledVideoFrame.data, m_scaledVideoFrame.linesize);
    //m_scaledVideoFrame.pkt_dts = m_decodedVideoFrame->pkt_dts;
    //m_scaledVideoFrame.pkt_pts = m_decodedVideoFrame->pkt_pts;
    m_scaledVideoFrame.pts = m_decodedVideoFrame->pts;
    return 0;
}

bool QnFfmpegVideoTranscoder::open(QnCompressedVideoDataPtr video)
{
    QnVideoTranscoder::open(video);

    m_videoDecoder = new CLFFmpegVideoDecoder(video->compressionType, video, m_mtMode);

    AVCodec* avCodec = avcodec_find_encoder(m_codecId);
    if (avCodec == 0)
    {
        m_lastErrMessage = QObject::tr("Transcoder error: can't find encoder for codec %1").arg(m_codecId);
        return false;
    }

    m_encoderCtx = avcodec_alloc_context3(avCodec);
    m_encoderCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    m_encoderCtx->codec_id = m_codecId;
    m_encoderCtx->width = m_resolution.width();
    m_encoderCtx->height = m_resolution.height();
    m_encoderCtx->pix_fmt = m_codecId == CODEC_ID_MJPEG ? PIX_FMT_YUVJ420P : PIX_FMT_YUV420P;
    m_encoderCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
    //m_encoderCtx->flags |= CODEC_FLAG_LOW_DELAY;
    if (m_bitrate == -1)
        m_bitrate = QnTranscoder::suggestBitrate(QSize(m_encoderCtx->width,m_encoderCtx->height), m_quality);
    m_encoderCtx->bit_rate = m_bitrate;
    m_encoderCtx->gop_size = 32;
    m_encoderCtx->time_base.num = 1;
    m_encoderCtx->time_base.den = 60;
    m_encoderCtx->sample_aspect_ratio.den = m_encoderCtx->sample_aspect_ratio.num = 1;
    if (m_mtMode)
        m_encoderCtx->thread_count = QThread::idealThreadCount();
    if (avcodec_open2(m_encoderCtx, avCodec, 0) < 0)
    {
        m_lastErrMessage = QObject::tr("Can't initialize video encoder");
        return false;
    }
    return true;
}

void QnFfmpegVideoTranscoder::initTimeDrawing(CLVideoDecoderOutput* frame, const QString& timeStr)
{
    m_timeFont.setBold(true);
    m_timeFont.setPixelSize(qMax(MIN_TEXT_HEIGHT, frame->height / TEXT_HEIGHT_IN_FRAME_PARTS));
    QFontMetrics metric(m_timeFont);
    //m_bufYOffs;

    switch(m_dateTextPos)
    {
    case Date_LeftTop:
        m_bufYOffs = 0;
        m_dateTimeXOffs = metric.averageCharWidth()/2;
        break;
    case Date_RightTop:
        m_bufYOffs = 0;
        m_dateTimeXOffs = frame->width - metric.width(timeStr) - metric.averageCharWidth()/2;
        break;
    case Date_RightBottom:
        m_bufYOffs = frame->height - metric.height();
        m_dateTimeXOffs = frame->width - metric.boundingRect(timeStr).width() - metric.averageCharWidth()/2; // - metric.width(QLatin1String("0"));
        break;
    case Date_LeftBottom:
    default:
        m_bufYOffs = frame->height - metric.height();
        m_dateTimeXOffs = metric.averageCharWidth()/2;
        break;
    }

    m_bufYOffs = qPower2Floor(m_bufYOffs, 2);
    m_bufXOffs = qPower2Floor(m_dateTimeXOffs, CL_MEDIA_ALIGNMENT);

    m_dateTimeXOffs = m_dateTimeXOffs%CL_MEDIA_ALIGNMENT;
    m_dateTimeYOffs = metric.ascent();

    int drawWidth = metric.width(timeStr);
    int drawHeight = metric.height();
    drawWidth = qPower2Ceil((unsigned) drawWidth + m_dateTimeXOffs, CL_MEDIA_ALIGNMENT);
    m_imageBuffer = (uchar*) qMallocAligned(drawWidth * drawHeight * 4, CL_MEDIA_ALIGNMENT);
    m_timeImg = new QImage(m_imageBuffer, drawWidth, drawHeight, drawWidth*4, QImage::Format_ARGB32_Premultiplied);
}

void QnFfmpegVideoTranscoder::doDrawOnScreenTime(CLVideoDecoderOutput* frame)
{
    QString timeStr;
    qint64 displayTime = frame->pts/1000 + m_onscreenDateOffset;
    if (frame->pts >= UTC_TIME_DETECTION_THRESHOLD)
        timeStr = QDateTime::fromMSecsSinceEpoch(displayTime).toString(lit("yyyy-MMM-dd hh:mm:ss"));
    else
        timeStr = QTime().addMSecs(displayTime).toString(lit("hh:mm:ss.zzz"));

    if (m_timeImg == 0)
        initTimeDrawing(frame, timeStr);

    int bufPlaneYOffs  = m_bufXOffs + m_bufYOffs * frame->linesize[0];
    int bufferUVOffs = m_bufXOffs/2 + m_bufYOffs * frame->linesize[1] / 2;

    // copy and convert frame buffer to image
    yuv420_argb32_sse2_intr(m_imageBuffer,
        frame->data[0]+bufPlaneYOffs, frame->data[1]+bufferUVOffs, frame->data[2]+bufferUVOffs,
        m_timeImg->width(), m_timeImg->height(),
        m_timeImg->bytesPerLine(), 
        frame->linesize[0], frame->linesize[1], 255);

    QPainter p(m_timeImg);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    QPainterPath path;
    path.addText(m_dateTimeXOffs, m_dateTimeYOffs, m_timeFont, timeStr);
    p.setBrush(Qt::white);
    p.drawPath(path);
    p.strokePath(path, QPen(QColor(32,32,32,80)));

    // copy and convert RGBA32 image back to frame buffer
    bgra_to_yv12_sse2_intr(m_imageBuffer, m_timeImg->bytesPerLine(), 
        frame->data[0]+bufPlaneYOffs, frame->data[1]+bufferUVOffs, frame->data[2]+bufferUVOffs,
        frame->linesize[0], frame->linesize[1], 
        m_timeImg->width(), m_timeImg->height(), false);
}

int QnFfmpegVideoTranscoder::transcodePacket(QnAbstractMediaDataPtr media, QnAbstractMediaDataPtr* const result)
{
    if( result )
        result->clear();
    if (!media)
        return 0;

    if (!m_lastErrMessage.isEmpty())
        return -3;

    QnCompressedVideoDataPtr video = qSharedPointerDynamicCast<QnCompressedVideoData>(media);
    if (m_videoDecoder->decode(video, &m_decodedVideoFrame)) 
    {
        CLVideoDecoderOutput* decodedFrame = m_decodedVideoFrame.data();
        m_decodedVideoFrame->pts = m_decodedVideoFrame->pkt_dts;
        if (m_decodedVideoFrame->width != m_resolution.width() || m_decodedVideoFrame->height != m_resolution.height() || m_decodedVideoFrame->format != PIX_FMT_YUV420P) {
            rescaleFrame();
            decodedFrame = &m_scaledVideoFrame;
        }

        if (m_dateTextPos != Date_None)
            doDrawOnScreenTime(decodedFrame);

        static AVRational r = {1, 1000000};
        decodedFrame->pts  = av_rescale_q(decodedFrame->pts, r, m_encoderCtx->time_base);
        if ((quint64)m_firstEncodedPts == AV_NOPTS_VALUE)
            m_firstEncodedPts = decodedFrame->pts;

        if( !result )
            return 0;

        //TODO: #vasilenko avoid using deprecated methods
        int encoded = avcodec_encode_video(m_encoderCtx, m_videoEncodingBuffer, MAX_VIDEO_FRAME, decodedFrame);

        if (encoded < 0)
        {
            return -3;
        }
        *result = QnCompressedVideoDataPtr(new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, encoded));
        (*result)->timestamp = av_rescale_q(m_encoderCtx->coded_frame->pts, m_encoderCtx->time_base, r);
        if(m_encoderCtx->coded_frame->key_frame)
            (*result)->flags |= AV_PKT_FLAG_KEY;
        (*result)->data.write((const char*) m_videoEncodingBuffer, encoded); // todo: remove data copy here!
        return 0;
    }
    else {
        if( result )
            *result = QnCompressedVideoDataPtr();
        return 0; // ignore decode error
    }
}

AVCodecContext* QnFfmpegVideoTranscoder::getCodecContext()
{
    return m_encoderCtx;
}

void QnFfmpegVideoTranscoder::setMTMode(bool value)
{
    m_mtMode = value;
}


void QnFfmpegVideoTranscoder::setDrawDateTime(OnScreenDatePos value)
{
    m_dateTextPos = value;
}

void QnFfmpegVideoTranscoder::setQuality(QnStreamQuality quality)
{
    m_quality = quality;
}

void QnFfmpegVideoTranscoder::setOnScreenDateOffset(int timeOffsetMs)
{
    m_onscreenDateOffset = timeOffsetMs;
}
