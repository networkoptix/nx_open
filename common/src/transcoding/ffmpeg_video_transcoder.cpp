
#include "ffmpeg_video_transcoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include "core/datapacket/video_data_packet.h"
#include "decoders/video/ffmpeg.h"
#include "filters/abstract_image_filter.h"
#include "filters/crop_image_filter.h"
#include "utils/common/util.h"

const static int MAX_VIDEO_FRAME = 1024 * 1024 * 3;
const static qint64 OPTIMIZATION_BEGIN_FRAME = 10;

QnFfmpegVideoTranscoder::QnFfmpegVideoTranscoder(CodecID codecId):
    QnVideoTranscoder(codecId),
    m_decodedVideoFrame(new CLVideoDecoderOutput()),
    m_encoderCtx(0),
    m_firstEncodedPts(AV_NOPTS_VALUE),
    m_mtMode(false),
    m_lastEncodedTime(AV_NOPTS_VALUE),
    m_totalSpentTime(0),
    m_totalEncodedTime(0),
    m_totalDecodedFrames(0),
    m_totalEncodedFrames(0),
    m_droppedFrames(0)
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i) 
    {
        m_lastSrcWidth[i] = -1;
        m_lastSrcHeight[i] = -1;
    }
    m_videoDecoders.resize(CL_MAX_CHANNELS);
    m_videoEncodingBuffer = (quint8*) qMallocAligned(MAX_VIDEO_FRAME, 32);
    m_decodedVideoFrame->setUseExternalData(true);
}

QnFfmpegVideoTranscoder::~QnFfmpegVideoTranscoder()
{
    qFreeAligned(m_videoEncodingBuffer);
    close();
}

void QnFfmpegVideoTranscoder::close()
{
    QnFfmpegHelper::deleteCodecContext(m_encoderCtx);
    m_encoderCtx = 0;

    for (int i = 0; i < m_videoDecoders.size(); ++i) {
        delete m_videoDecoders[i];
        m_videoDecoders[i] = 0;
    }
}

bool QnFfmpegVideoTranscoder::open(const QnConstCompressedVideoDataPtr& video)
{
    close();

    QnVideoTranscoder::open(video);

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
        m_encoderCtx->thread_count = qMin(2, QThread::idealThreadCount());

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

    m_encodeTimer.start();
    return true;
}

int QnFfmpegVideoTranscoder::transcodePacket(const QnConstAbstractMediaDataPtr& media, QnAbstractMediaDataPtr* const result)
{
    m_encodeTimer.restart();

    if( result )
        result->reset();
    if (!media)
        return 0;

    if (!m_lastErrMessage.isEmpty())
        return -3;

    const auto video = std::dynamic_pointer_cast<const QnCompressedVideoData>(media);
    if (auto ret = transcodePacketImpl(video, result))
        return ret;
    
    if (m_lastEncodedTime != AV_NOPTS_VALUE)
    {
        const auto thisFrameTime = video->timestamp - m_lastEncodedTime;
        m_totalEncodedTime += (thisFrameTime > MAX_FRAME_DURATION * 1000) ?
            (m_totalEncodedTime / m_totalEncodedFrames) : thisFrameTime;
    }
    
    m_lastEncodedTime = video->timestamp;
    m_totalSpentTime += m_encodeTimer.elapsed() * 1000; // mks
    return 0;
}

int QnFfmpegVideoTranscoder::transcodePacketImpl(const QnConstCompressedVideoDataPtr& video, QnAbstractMediaDataPtr* const result)
{
    
    CLFFmpegVideoDecoder* decoder = m_videoDecoders[video->channelNumber];
    if (!decoder)
        decoder = m_videoDecoders[video->channelNumber] = new CLFFmpegVideoDecoder(video->compressionType, video, m_mtMode);

    if (decoder->decode(video, &m_decodedVideoFrame)) 
    {
        ++m_totalDecodedFrames;

        m_decodedVideoFrame->channel = video->channelNumber;
        CLVideoDecoderOutputPtr decodedFrame = m_decodedVideoFrame;

        decodedFrame->pts = m_decodedVideoFrame->pkt_dts;
        decodedFrame = processFilterChain(decodedFrame);

        if (decodedFrame->width != m_resolution.width() || decodedFrame->height != m_resolution.height() || decodedFrame->format != PIX_FMT_YUV420P) 
            decodedFrame = CLVideoDecoderOutputPtr(decodedFrame->scaled(m_resolution, PIX_FMT_YUV420P));

        static AVRational r = {1, 1000000};
        decodedFrame->pts  = av_rescale_q(decodedFrame->pts, r, m_encoderCtx->time_base);
        if ((quint64)m_firstEncodedPts == AV_NOPTS_VALUE)
            m_firstEncodedPts = decodedFrame->pts;

        if( !result )
            return 0;

        // Improve performance by omitting frames to encode in case if encoding goes way too slow..
        // TODO: #mu make mediaserver's config option and HTTP query option to disable feature
        if (m_totalEncodedFrames > OPTIMIZATION_BEGIN_FRAME)
        {   
            // the more frames are dropped the lower limit is gonna be set
            // (Xi + (Xi >> 2)) is faster and more precise then (Xi * 1.25f
            const auto spentTime = m_totalSpentTime + (m_totalSpentTime >> m_droppedFrames);
            if (spentTime > m_totalEncodedTime)
            {
                // calculate approved frames rateable to delay
                const auto framesToEncode = m_totalEncodedFrames * m_totalEncodedTime / spentTime;
                if (m_totalEncodedFrames > framesToEncode)
                {
                    ++m_droppedFrames;
                    return 0;
                }
            }
        }

        //TODO: #vasilenko avoid using deprecated methods
        int encoded = avcodec_encode_video(m_encoderCtx, m_videoEncodingBuffer, MAX_VIDEO_FRAME, decodedFrame.data());
        if (encoded < 0)
            return -3;

        QnWritableCompressedVideoData* resultVideoData = new QnWritableCompressedVideoData(CL_MEDIA_ALIGNMENT, encoded);
        resultVideoData->timestamp = av_rescale_q(m_encoderCtx->coded_frame->pts, m_encoderCtx->time_base, r);
        if(m_encoderCtx->coded_frame->key_frame)
            resultVideoData->flags |= QnAbstractMediaData::MediaFlags_AVKey;
        resultVideoData->m_data.write((const char*) m_videoEncodingBuffer, encoded); // todo: remove data copy here!
        *result = QnCompressedVideoDataPtr(resultVideoData);
        
        ++m_totalEncodedFrames;
        m_droppedFrames = 0;
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

void QnFfmpegVideoTranscoder::setFilterList(QList<QnAbstractImageFilterPtr> filters)
{
    QnVideoTranscoder::setFilterList(filters);
    m_decodedVideoFrame->setUseExternalData(false); // do not modify ffmpeg frame buffer
}

#endif // ENABLE_DATA_PROVIDERS
