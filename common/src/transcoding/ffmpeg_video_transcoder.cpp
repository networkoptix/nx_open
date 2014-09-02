
#include "ffmpeg_video_transcoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include "core/datapacket/video_data_packet.h"
#include "decoders/video/ffmpeg.h"

extern "C" {
#ifdef WIN32
#define AVPixFmtDescriptor __declspec(dllimport) AVPixFmtDescriptor
#endif
#include <libavutil/pixdesc.h>
#ifdef WIN32
#undef AVPixFmtDescriptor
#endif
};

const static int MAX_VIDEO_FRAME = 1024 * 1024 * 3;

QnFfmpegVideoTranscoder::QnFfmpegVideoTranscoder(CodecID codecId):
    QnVideoTranscoder(codecId),
    m_decodedVideoFrame(new CLVideoDecoderOutput()),
    m_encoderCtx(0),
    m_firstEncodedPts(AV_NOPTS_VALUE),
    m_mtMode(false)
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i) 
    {
        scaleContext[i] = 0;
        m_lastSrcWidth[i] = -1;
        m_lastSrcHeight[i] = -1;
    }

    m_videoEncodingBuffer = (quint8*) qMallocAligned(MAX_VIDEO_FRAME, 32);
    m_decodedVideoFrame->setUseExternalData(true);
    m_decodedFrameRect.setUseExternalData(true);
}

QnFfmpegVideoTranscoder::~QnFfmpegVideoTranscoder()
{
    qFreeAligned(m_videoEncodingBuffer);
    close();
}

void QnFfmpegVideoTranscoder::close()
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i) {
        if (scaleContext[i])
            sws_freeContext(scaleContext[i]);
    }

    if (m_encoderCtx) {
        avcodec_close(m_encoderCtx);
        av_free(m_encoderCtx);
        m_encoderCtx = 0;
    }

    for (int i = 0; i < m_videoDecoders.size(); ++i)
        delete m_videoDecoders[i];
    m_videoDecoders.clear();
}

int QnFfmpegVideoTranscoder::rescaleFrame(CLVideoDecoderOutput* decodedFrame, const QRectF& dstRectF, int ch)
{
    if (m_scaledVideoFrame.width == 0) {
        m_scaledVideoFrame.reallocate(m_resolution.width(), m_resolution.height(), PIX_FMT_YUV420P);
        m_scaledVideoFrame.memZerro();
    }

    QRect dstRect(0,0, m_resolution.width(), m_resolution.height());
    quint8* dstData[4];
    for (int i = 0; i < 4; ++i)
        dstData[i] = m_scaledVideoFrame.data[i];

    if (m_layout)
    {
        dstRect = QRect(dstRectF.left() * m_resolution.width() + 0.5, dstRectF.top() * m_resolution.height() + 0.5,
                        dstRectF.width() * m_resolution.width() + 0.5, dstRectF.height() * m_resolution.height() + 0.5);
        dstRect = roundRect(dstRect);

        for (int i = 0; i < 3; ++i)
        {
            int w = dstRect.left();
            if (i > 0)
                w >>= 1;
            dstData[i] += w + dstRect.top() * m_scaledVideoFrame.linesize[i];
        }
    }

    if (decodedFrame->width != m_lastSrcWidth[ch] ||  decodedFrame->height != m_lastSrcHeight[ch])
    {
        // src resolution is changed
        m_lastSrcWidth[ch] = decodedFrame->width;
        m_lastSrcHeight[ch] = decodedFrame->height;
        if (scaleContext[ch]) {
            sws_freeContext(scaleContext[ch]);
            scaleContext[ch] = 0;
        }
    }

    if (scaleContext[ch] == 0)
    {
        scaleContext[ch] = sws_getContext(decodedFrame->width, decodedFrame->height, (PixelFormat) decodedFrame->format, 
                                      dstRect.width(), dstRect.height(), (PixelFormat) PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);
        if (!scaleContext) {
            m_lastErrMessage = tr("Could not allocate scaler context for resolution %1x%2.").arg(m_resolution.width()).arg(m_resolution.height());
            return -4;
        }
    }

    sws_scale(scaleContext[ch],
        decodedFrame->data, decodedFrame->linesize, 
        0, decodedFrame->height, 
        dstData, m_scaledVideoFrame.linesize);
    return 0;
}

bool QnFfmpegVideoTranscoder::open(const QnConstCompressedVideoDataPtr& video)
{
    close();

    QnVideoTranscoder::open(video);

    int channels = m_layout ? m_layout->channelCount() : 1;
    for (int i = 0; i < channels; ++i)
        m_videoDecoders << new CLFFmpegVideoDecoder(video->compressionType, video, m_mtMode);

    AVCodec* avCodec = avcodec_find_encoder(m_codecId);
    if (avCodec == 0)
    {
        m_lastErrMessage = tr("Could not find encoder for codec %1.").arg(m_codecId);
        return false;
    }

    m_encoderCtx = avcodec_alloc_context3(avCodec);
    m_encoderCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    m_encoderCtx->codec_id = m_codecId;
    m_encoderCtx->width = m_resolution.width();
    m_encoderCtx->height = m_resolution.height();
    m_encoderCtx->pix_fmt = m_codecId == CODEC_ID_MJPEG ? PIX_FMT_YUVJ420P : PIX_FMT_YUV420P;
    m_encoderCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
    if (m_bitrate == -1)
        m_bitrate = QnTranscoder::suggestMediaStreamParams( m_codecId, QSize(m_encoderCtx->width,m_encoderCtx->height), m_quality );
    m_encoderCtx->bit_rate = m_bitrate;
    m_encoderCtx->gop_size = 32;
    m_encoderCtx->time_base.num = 1;
    m_encoderCtx->time_base.den = 60;
    m_encoderCtx->sample_aspect_ratio.den = m_encoderCtx->sample_aspect_ratio.num = 1;
    if (m_mtMode)
        m_encoderCtx->thread_count = QThread::idealThreadCount();

    for( QMap<QString, QVariant>::const_iterator it = m_params.begin(); it != m_params.end(); ++it )
    {
        if( it.key() == QnCodecParams::global_quality )
        {
            //100 - 1;
            //0 - FF_LAMBDA_MAX;
            m_encoderCtx->global_quality = INT_MAX / 50 * (it.value().toInt() - 50);
        }
        else if( it.key() == QnCodecParams::qscale )
        {
            m_encoderCtx->flags |= CODEC_FLAG_QSCALE;
            m_encoderCtx->global_quality = FF_QP2LAMBDA * it.value().toInt();
        }
        else if( it.key() == QnCodecParams::qmin )
        {
            m_encoderCtx->qmin = it.value().toInt();
        }
        else if( it.key() == QnCodecParams::qmax )
        {
            m_encoderCtx->qmax = it.value().toInt();
        }
    }


    if (avcodec_open2(m_encoderCtx, avCodec, 0) < 0)
    {
        m_lastErrMessage = tr("Could not initialize video encoder.");
        return false;
    }

    return true;
}

int QnFfmpegVideoTranscoder::transcodePacket(const QnConstAbstractMediaDataPtr& media, QnAbstractMediaDataPtr* const result)
{
    if( result )
        result->clear();
    if (!media)
        return 0;

    if (!m_lastErrMessage.isEmpty())
        return -3;


    QnConstCompressedVideoDataPtr video = qSharedPointerDynamicCast<const QnCompressedVideoData>(media);
    CLFFmpegVideoDecoder* decoder = m_videoDecoders[m_layout ? video->channelNumber : 0];
    QRectF dstRectF(0,0, 1,1);

    if (decoder->decode(video, &m_decodedVideoFrame)) 
    {

        QRect frameRect;
        if (m_layout)
        {
            QPoint pos = m_layout->position(video->channelNumber);
            QSize lSize = m_layout->size();
            if (!m_srcRectF.isEmpty())
            {
                QRectF srcRectF(m_srcRectF.left() * lSize.width(), m_srcRectF.top() * lSize.height(),
                                m_srcRectF.width() * lSize.width(), m_srcRectF.height() * lSize.height());
                QRectF channelRect(pos.x(), pos.y(), 1, 1);

                if (!channelRect.intersects(srcRectF))
                    return 0; // channel outside bounding rect

                QRectF frameRectF = srcRectF.intersected(channelRect);

                qreal dstWidth = frameRectF.width() / srcRectF.width();
                qreal dstHeight = frameRectF.height() / srcRectF.height();
                qreal dstLeft = (frameRectF.left() - srcRectF.left())/srcRectF.width();
                qreal dstTop = (frameRectF.top() - srcRectF.top())/srcRectF.height();
                dstRectF = QRectF(dstLeft, dstTop, dstWidth, dstHeight);

                frameRectF.translate(-channelRect.left(), -channelRect.top());
                dstRectF.setRight(qMin<double>(channelRect.right(), 1.0)); // avoid epsilon factor
                dstRectF.setBottom(qMin<double>(channelRect.bottom(), 1.0));
                frameRect = QRect(frameRectF.left() * decoder->getWidth() + 0.5, frameRectF.top() * decoder->getHeight() + 0.5, 
                    frameRectF.width() * decoder->getWidth() + 0.5, frameRectF.height() * decoder->getHeight() + 0.5);
                frameRect = roundRect(frameRect);
            }
            else {
                dstRectF = QRectF(pos.x() / (qreal) lSize.width(), pos.y() / (qreal) lSize.height(),
                            1.0 / lSize.width(), 1.0 / lSize.height());
            }
        }
        CLVideoDecoderOutput* decodedFrame = m_decodedVideoFrame.data();

        if (!frameRect.isEmpty()) 
        {
            const AVPixFmtDescriptor* descr = &av_pix_fmt_descriptors[decodedFrame->format];
            for (int i = 0; i < descr->nb_components && decodedFrame->data[i]; ++i)
            {
                int w = frameRect.left();
                int h = frameRect.top();
                if (i > 0) {
                    w >>= descr->log2_chroma_w;
                    h >>= descr->log2_chroma_h;
                }
                m_decodedFrameRect.data[i] = decodedFrame->data[i] + w + h * decodedFrame->linesize[i];
                m_decodedFrameRect.linesize[i] = decodedFrame->linesize[i];
            }
            m_decodedFrameRect.format = decodedFrame->format;
            m_decodedFrameRect.width = frameRect.width();
            m_decodedFrameRect.height = frameRect.height();
            decodedFrame = &m_decodedFrameRect;
        }

        if (decodedFrame->width != m_resolution.width() || decodedFrame->height != m_resolution.height() || decodedFrame->format != PIX_FMT_YUV420P) {
            rescaleFrame(decodedFrame, dstRectF, video->channelNumber);
            decodedFrame = &m_scaledVideoFrame;
        }
        decodedFrame->pts = m_decodedVideoFrame->pkt_dts;
        qreal ar = decoder->getWidth() * (qreal) decoder->getSampleAspectRatio() / (qreal) decoder->getHeight();
        processFilterChain(decodedFrame, dstRectF, ar);

        static AVRational r = {1, 1000000};
        decodedFrame->pts  = av_rescale_q(m_decodedVideoFrame->pkt_dts, r, m_encoderCtx->time_base);
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
        QnWritableCompressedVideoData* resultVideoData = new QnWritableCompressedVideoData(CL_MEDIA_ALIGNMENT, encoded);
        resultVideoData->timestamp = av_rescale_q(m_encoderCtx->coded_frame->pts, m_encoderCtx->time_base, r);
        if(m_encoderCtx->coded_frame->key_frame)
            resultVideoData->flags |= QnAbstractMediaData::MediaFlags_AVKey;
        resultVideoData->m_data.write((const char*) m_videoEncodingBuffer, encoded); // todo: remove data copy here!
        *result = QnCompressedVideoDataPtr(resultVideoData);
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

void QnFfmpegVideoTranscoder::addFilter(QnAbstractImageFilter* filter)
{
    QnVideoTranscoder::addFilter(filter);
    m_decodedVideoFrame->setUseExternalData(false); // do not modify ffmpeg frame buffer
}

#endif // ENABLE_DATA_PROVIDERS
