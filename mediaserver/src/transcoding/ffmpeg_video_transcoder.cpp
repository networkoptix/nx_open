#include "ffmpeg_video_transcoder.h"
#include "decoders/video/ffmpeg.h"

const static int MAX_VIDEO_FRAME = 1024 * 1024 * 3;

QnFfmpegVideoTranscoder::QnFfmpegVideoTranscoder(CodecID codecId):
QnVideoTranscoder(codecId),
m_videoDecoder(0),
scaleContext(0),
m_encoderCtx(0),
m_firstEncodedPts(AV_NOPTS_VALUE)
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
}

int QnFfmpegVideoTranscoder::rescaleFrame()
{
    if (scaleContext == 0)
    {
        scaleContext = sws_getContext(m_decodedVideoFrame.width, m_decodedVideoFrame.height, (PixelFormat) m_decodedVideoFrame.format, 
                                      m_resolution.width(), m_resolution.height(), (PixelFormat) PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);
        if (!scaleContext) {
            m_lastErrMessage = QString("Can't allocate scaler context for resolution %1x%2").arg(m_resolution.width()).arg(m_resolution.height());
            return -4;
        }
        m_scaledVideoFrame.reallocate(m_resolution.width(), m_resolution.height(), PIX_FMT_YUV420P);
    }
    sws_scale(scaleContext,
        m_decodedVideoFrame.data, m_decodedVideoFrame.linesize, 
        0, m_decodedVideoFrame.height, 
        m_scaledVideoFrame.data, m_scaledVideoFrame.linesize);
    //m_scaledVideoFrame.pkt_dts = m_decodedVideoFrame.pkt_dts;
    //m_scaledVideoFrame.pkt_pts = m_decodedVideoFrame.pkt_pts;
    m_scaledVideoFrame.pts = m_decodedVideoFrame.pts;
    return 0;
}

int QnFfmpegVideoTranscoder::transcodePacket(QnAbstractMediaDataPtr media, QnAbstractMediaDataPtr& result)
{
    QnCompressedVideoDataPtr video = qSharedPointerDynamicCast<QnCompressedVideoData>(media);
    if (!m_videoDecoder) {
        m_videoDecoder = new CLFFmpegVideoDecoder(video->compressionType, video, false);

        AVCodec* avCodec = avcodec_find_encoder(m_codecId);
        if (avCodec == 0)
        {
            m_lastErrMessage = QObject::tr("Transcoder error: can't find encoder for codec %1").arg(m_codecId);
            return -2;
        }

        m_encoderCtx = avcodec_alloc_context3(avCodec);
        m_encoderCtx->codec_type = AVMEDIA_TYPE_VIDEO;
        m_encoderCtx->codec_id = m_codecId;
        m_encoderCtx->width = m_resolution.width();
        m_encoderCtx->height = m_resolution.height();
        m_encoderCtx->pix_fmt = PIX_FMT_YUV420P;
        m_encoderCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
        m_encoderCtx->flags |= CODEC_FLAG_LOW_DELAY;

        m_encoderCtx->bit_rate = m_bitrate;
        m_encoderCtx->gop_size = 32;
        m_encoderCtx->time_base.num = 1;
        m_encoderCtx->time_base.den = 60;
        m_encoderCtx->sample_aspect_ratio.den = m_encoderCtx->sample_aspect_ratio.num = 1;
        if (avcodec_open2(m_encoderCtx, avCodec, 0) < 0)
        {
            m_lastErrMessage = QString("Can't initialize video encoder");
            return -3;
        }
    }
    if (m_videoDecoder->decode(video, &m_decodedVideoFrame)) 
    {
        CLVideoDecoderOutput* decodedFrame = &m_decodedVideoFrame;
        m_decodedVideoFrame.pts = m_decodedVideoFrame.pkt_dts;
        if (m_decodedVideoFrame.width != m_resolution.width() || m_decodedVideoFrame.height != m_resolution.height() || m_decodedVideoFrame.format != PIX_FMT_YUV420P) {
            rescaleFrame();
            decodedFrame = &m_scaledVideoFrame;
        }

        static AVRational r = {1, 1000000};
        decodedFrame->pts  = av_rescale_q(decodedFrame->pts, r, m_encoderCtx->time_base);
        if (m_firstEncodedPts == AV_NOPTS_VALUE)
            m_firstEncodedPts = decodedFrame->pts;
        decodedFrame->pts -= m_firstEncodedPts;


        int encoded = avcodec_encode_video(m_encoderCtx, m_videoEncodingBuffer, MAX_VIDEO_FRAME, decodedFrame);

        if (encoded < 0)
        {
            return -3;
        }
        result = QnCompressedVideoDataPtr(new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, encoded));
        result->timestamp = av_rescale_q(m_encoderCtx->coded_frame->pts, m_encoderCtx->time_base, r);
        if(m_encoderCtx->coded_frame->key_frame)
            result->flags |= AV_PKT_FLAG_KEY;
        result->data.write((const char*) m_videoEncodingBuffer, encoded); // todo: remove data copy here!
        return 0;
    }
    else {
        result = QnCompressedVideoDataPtr();
        return 0; // ignore decode error
    }
}
