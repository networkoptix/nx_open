
#include "ffmpeg_video_transcoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include "nx/streaming/video_data_packet.h"
#include "decoders/video/ffmpeg_video_decoder.h"
#include "filters/abstract_image_filter.h"
#include "filters/crop_image_filter.h"
#include "utils/common/util.h"
#include <nx/streaming/av_codec_media_context.h>
#include <nx/streaming/config.h>

namespace {
const static int MAX_VIDEO_FRAME = 1024 * 1024 * 3;
const static qint64 OPTIMIZATION_BEGIN_FRAME = 10;
const static qint64 OPTIMIZATION_MOVING_AVERAGE_RATE = 90;
static const int kMaxDroppedFrames = 5;
}

namespace {

    static qint64& movigAverage(qint64& accumulator, qint64 value)
    {
        static_assert(OPTIMIZATION_MOVING_AVERAGE_RATE < 100, "Moving average K should be below 100%");
        static const auto movingAvg = static_cast<double>(OPTIMIZATION_MOVING_AVERAGE_RATE) / 100.0;

        if (accumulator == 0)
            return accumulator = value;

        return accumulator = accumulator * movingAvg + value * (1.0 - movingAvg);
    }

    AVCodecID updateCodec(AVCodecID codec)
    {
        if (codec == AV_CODEC_ID_H263P)
            return AV_CODEC_ID_H263;
        else
            return codec;
    }
}

QnFfmpegVideoTranscoder::QnFfmpegVideoTranscoder(AVCodecID codecId):
    QnVideoTranscoder(codecId),
    m_decodedVideoFrame(new CLVideoDecoderOutput()),
    m_encoderCtx(0),
    m_firstEncodedPts(AV_NOPTS_VALUE),
    m_mtMode(false),
    m_lastEncodedTime(AV_NOPTS_VALUE),
    m_averageCodingTimePerFrame(0),
    m_averageVideoTimePerFrame(0),
    m_encodedFrames(0),
    m_droppedFrames(0),
    m_useRealTimeOptimization(false),
    m_outPacket(av_packet_alloc())
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
    av_packet_free(&m_outPacket);
}

void QnFfmpegVideoTranscoder::close()
{
    QnFfmpegHelper::deleteAvCodecContext(m_encoderCtx);
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
    m_encoderCtx->pix_fmt = m_codecId == AV_CODEC_ID_MJPEG ? AV_PIX_FMT_YUVJ420P : AV_PIX_FMT_YUV420P;
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

    if (m_lastEncodedTime != qint64(AV_NOPTS_VALUE))
        movigAverage(m_averageVideoTimePerFrame, video->timestamp - m_lastEncodedTime);
    m_lastEncodedTime = video->timestamp;

    movigAverage(m_averageCodingTimePerFrame, m_encodeTimer.elapsed() * 1000 /*mks*/);
    return 0;
}

int QnFfmpegVideoTranscoder::transcodePacketImpl(const QnConstCompressedVideoDataPtr& video, QnAbstractMediaDataPtr* const result)
{

    if (!m_encoderCtx)
        open(video);
    if (!m_encoderCtx)
        return -3; //< encode error

    QnFfmpegVideoDecoder* decoder = m_videoDecoders[video->channelNumber];
    if (!decoder)
        decoder = m_videoDecoders[video->channelNumber] = new QnFfmpegVideoDecoder(video->compressionType, video, m_mtMode);

    if (result)
        *result = QnCompressedVideoDataPtr();

    if (!decoder->decode(video, &m_decodedVideoFrame))
        return 0; // ignore decode error

    if (video->flags.testFlag(QnAbstractMediaData::MediaFlags_Ignore))
        return 0; // do not transcode ignored frames

    m_decodedVideoFrame->channel = video->channelNumber;
    CLVideoDecoderOutputPtr decodedFrame = m_decodedVideoFrame;

    decodedFrame->pts = m_decodedVideoFrame->pkt_dts;

    // Second stream must be upscaled to the first stream resolution before the filters apply.
    if (decodedFrame->width != m_resolution.width() || decodedFrame->height != m_resolution.height() || decodedFrame->format != AV_PIX_FMT_YUV420P)
        decodedFrame = CLVideoDecoderOutputPtr(decodedFrame->scaled(m_resolution, AV_PIX_FMT_YUV420P));

    decodedFrame = processFilterChain(decodedFrame);

    static AVRational r = {1, 1000000};
    decodedFrame->pts  = av_rescale_q(decodedFrame->pts, r, m_encoderCtx->time_base);
    if (m_firstEncodedPts == AV_NOPTS_VALUE)
        m_firstEncodedPts = decodedFrame->pts;

    if( !result )
        return 0;

    // Improve performance by omitting frames to encode in case if encoding goes way too slow..
    // TODO: #mu make mediaserver's config option and HTTP query option to disable feature
    if (m_useRealTimeOptimization && m_encodedFrames > OPTIMIZATION_BEGIN_FRAME)
    {
        // the more frames are dropped the lower limit is gonna be set
        // (x + (x >> k)) is faster and more precise then (x * (1 + 0.5^k))
        const auto& codingTime = m_averageCodingTimePerFrame;
        if (codingTime + (codingTime >> m_droppedFrames) > m_averageVideoTimePerFrame)
        {
            if (++m_droppedFrames < kMaxDroppedFrames)
                return 0;
        }
    }

    // TODO: ffmpeg-test
    //int encoded = avcodec_encode_video(m_encoderCtx, m_videoEncodingBuffer, MAX_VIDEO_FRAME, decodedFrame.data());

    m_outPacket->data = m_videoEncodingBuffer;
    m_outPacket->size = MAX_VIDEO_FRAME;
    int got_packet = 0;
    int encodeResult = avcodec_encode_video2(m_encoderCtx, m_outPacket, decodedFrame.data(), &got_packet);
    if (encodeResult < 0)
        return -3;
    if (!got_packet)
        return 0;

    QnWritableCompressedVideoData* resultVideoData = new QnWritableCompressedVideoData(CL_MEDIA_ALIGNMENT, m_outPacket->size);
    resultVideoData->timestamp = av_rescale_q(m_encoderCtx->coded_frame->pts, m_encoderCtx->time_base, r);
    if(m_encoderCtx->coded_frame->key_frame)
        resultVideoData->flags |= QnAbstractMediaData::MediaFlags_AVKey;
    resultVideoData->m_data.write((const char*) m_videoEncodingBuffer, m_outPacket->size); // todo: remove data copy here!
    resultVideoData->compressionType = updateCodec(m_codecId);

    if (!m_ctxPtr)
        m_ctxPtr.reset(new QnAvCodecMediaContext(m_encoderCtx));
    resultVideoData->context = m_ctxPtr;
    resultVideoData->width = m_encoderCtx->width;
    resultVideoData->height = m_encoderCtx->height;
    *result = QnCompressedVideoDataPtr(resultVideoData);

    ++m_encodedFrames;
    m_droppedFrames = 0;
    return 0;
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

void QnFfmpegVideoTranscoder::setUseRealTimeOptimization(bool value)
{
    m_useRealTimeOptimization = value;
}

#endif // ENABLE_DATA_PROVIDERS
