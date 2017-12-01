#include "transcoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <utils/math/math.h>
#include <core/resource/media_resource.h>
#include <utils/media/sse_helper.h>

#include <transcoding/ffmpeg_transcoder.h>
#include <transcoding/ffmpeg_video_transcoder.h>
#include <transcoding/ffmpeg_audio_transcoder.h>

#include <transcoding/filters/abstract_image_filter.h>
#include <transcoding/filters/tiled_image_filter.h>
#include <transcoding/filters/scale_image_filter.h>
#include <transcoding/filters/rotate_image_filter.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/streaming/abstract_stream_data_provider.h>
#include <nx/streaming/config.h>
#include <core/resource/camera_resource.h>
#include <nx/fusion/serialization/json.h>

namespace {

static const int kWidthRoundingFactor = 16;
static const int kHeightRoundingFactor = 4;

} // namespace

// ---------------------------- QnCodecTranscoder ------------------
QnCodecTranscoder::QnCodecTranscoder(AVCodecID codecId)
:
    m_bitrate(-1),
    m_quality(Qn::QualityNormal)
{
    m_codecId = codecId;
}


void QnCodecTranscoder::setParams(const QnCodecParams::Value& params)
{
    m_params = params;
}

void QnCodecTranscoder::setBitrate(int value)
{
    m_bitrate = value;
}

int QnCodecTranscoder::getBitrate() const
{
    return m_bitrate;
}

/*
AVCodecContext* QnCodecTranscoder::getCodecContext()
{
    return 0;
}
*/

QString QnCodecTranscoder::getLastError() const
{
    return m_lastErrMessage;
}

void QnCodecTranscoder::setQuality( Qn::StreamQuality quality )
{
    m_quality = quality;
}

QRect QnCodecTranscoder::roundRect(const QRect& srcRect)
{
    int left = qPower2Floor((unsigned) srcRect.left(), CL_MEDIA_ALIGNMENT);
    int top = qPower2Floor((unsigned) srcRect.top(), 2);

    int width = qPower2Ceil((unsigned) srcRect.width(), 16);
    int height = qPower2Ceil((unsigned) srcRect.height(), 2);

    return QRect(left, top, width, height);
}

QSize QnCodecTranscoder::roundSize(const QSize& size)
{
    int width = qPower2Ceil((unsigned) size.width(), 16);
    int height = qPower2Ceil((unsigned) size.height(), 2);

    return QSize(width, height);
}

// --------------------------- QnVideoTranscoder -----------------

QnVideoTranscoder::QnVideoTranscoder(AVCodecID codecId):
    QnCodecTranscoder(codecId)
{

}

QnVideoTranscoder::~QnVideoTranscoder()
{
}

void QnVideoTranscoder::setResolution(const QSize& value)
{
    m_resolution = value;
}

void QnVideoTranscoder::setFilterList(QList<QnAbstractImageFilterPtr> filterList)
{
    m_filters = filterList;
}

CLVideoDecoderOutputPtr QnVideoTranscoder::processFilterChain(const CLVideoDecoderOutputPtr& decodedFrame)
{
    if (m_filters.isEmpty())
        return decodedFrame;
    CLVideoDecoderOutputPtr result = decodedFrame;
    for(QnAbstractImageFilterPtr filter: m_filters)
    {
        result = filter->updateImage(result);
        if (!result)
            break;
    }
    return result;
}

QSize QnVideoTranscoder::getResolution() const
{
    return m_resolution;
}

QSize findSavedResolution(const QnConstCompressedVideoDataPtr& video)
{
    QSize result;
    if (!video->dataProvider)
        return result;
    QnResourcePtr res = video->dataProvider->getResource();
    if (!res)
        return result;
    const CameraMediaStreams supportedMediaStreams = QJson::deserialized<CameraMediaStreams>(res->getProperty( Qn::CAMERA_MEDIA_STREAM_LIST_PARAM_NAME ).toLatin1() );
    for(const auto& stream: supportedMediaStreams.streams)
    {
        QStringList resolutionInfo = stream.resolution.split(L'x');
        if (resolutionInfo.size() == 2) {
            QSize newRes(resolutionInfo[0].toInt(), resolutionInfo[1].toInt());
            if (newRes.height() > result.height())
                result = newRes;
        }
    }
    return result;
}

bool QnVideoTranscoder::open(const QnConstCompressedVideoDataPtr& video)
{
    QnFfmpegVideoDecoder decoder(video->compressionType, video, false);
    QSharedPointer<CLVideoDecoderOutput> decodedVideoFrame( new CLVideoDecoderOutput() );
    decoder.decode(video, &decodedVideoFrame);
    if (m_resolution.width() == 0 && m_resolution.height() > 0)
    {
        QSize srcResolution = findSavedResolution(video);
        if (srcResolution.isEmpty())
            srcResolution = QSize(decoder.getContext()->width, decoder.getContext()->height);


        m_resolution.setHeight(qMin(srcResolution.height(), m_resolution.height())); // strict to source frame height
        // Round resolution height.
        m_resolution.setHeight(
            qPower2Round((unsigned) m_resolution.height(), kHeightRoundingFactor));

        float ar = srcResolution.width() / (float)srcResolution.height();
        m_resolution.setWidth(m_resolution.height() * ar);
        // Round resolution width.
        m_resolution.setWidth(qPower2Round((unsigned) m_resolution.width(), kWidthRoundingFactor));
    }
    else if ((m_resolution.width() == 0 && m_resolution.height() == 0) || m_resolution.isEmpty())
    {
        m_resolution = QSize(decoder.getContext()->width, decoder.getContext()->height);
    }

    for(auto filter: m_filters)
        setResolution(filter->updatedResolution(getResolution()));

    return true;
}

// ---------------------- QnTranscoder -------------------------


QnTranscoder::QnTranscoder():
    m_videoCodec(AV_CODEC_ID_NONE),
    m_audioCodec(AV_CODEC_ID_NONE),
    m_videoStreamCopy(false),
    m_audioStreamCopy(false),
    m_internalBuffer(CL_MEDIA_ALIGNMENT, 1024*1024),
    m_firstTime(AV_NOPTS_VALUE),
    m_initialized(false),
    m_initializedAudio(false),
    m_initializedVideo(false),
    m_eofCounter(0),
    m_packetizedMode(false),
    m_useRealTimeOptimization(false)
{
    QThread::currentThread()->setPriority(QThread::LowPriority);
}

QnTranscoder::~QnTranscoder()
{

}

int QnTranscoder::suggestMediaStreamParams(
    AVCodecID codec,
    QSize resolution,
    Qn::StreamQuality quality,
    QnCodecParams::Value* const params )
{
    // I assume for a Qn::QualityHighest quality 30 fps for 1080 we need 10 mbps
    // I assume for a Qn::QualityLowest quality 30 fps for 1080 we need 1 mbps

    if (resolution.width() == 0)
        resolution.setWidth(resolution.height()*4/3);

    int hiEnd;
    switch(quality)
    {
        case Qn::QualityLowest:
            hiEnd = 1024;
            break;
        case Qn::QualityLow:
            hiEnd = 1024 + 512;
            break;
        case Qn::QualityNormal:
            hiEnd = 1024*2;
            break;
        case Qn::QualityHigh:
            hiEnd = 1024*3;
            break;
        case Qn::QualityHighest:
        default:
            hiEnd = 1024*5;
            break;
    }

    float resolutionFactor = resolution.width()*resolution.height()/1920.0/1080;
    resolutionFactor = pow(resolutionFactor, (float)0.63); // 256kbps for 320x240 for Mq quality

    int result = hiEnd * resolutionFactor;

    if( codec == AV_CODEC_ID_MJPEG )
    {
        //setting qmin and qmax, since mjpeg encoder uses only these
        if( params && !params->contains(QnCodecParams::qmin) && !params->contains(QnCodecParams::qmax) )
        {
            int qVal = 1;
            switch( quality )
            {
                case Qn::QualityLowest:
                    qVal = 100;
                    break;
                case Qn::QualityLow:
                    qVal = 50;
                    break;
                case Qn::QualityNormal:
                    qVal = 20;
                    break;
                case Qn::QualityHigh:
                    qVal = 5;
                    break;
                case Qn::QualityHighest:
                    qVal = 1;
                    break;
                default:
                    break;
            }

            params->insert( QnCodecParams::qmin, qVal );
            params->insert( QnCodecParams::qmax, qVal );
        }
    }

    return qMax(128,result)*1024;
}

int QnTranscoder::setVideoCodec(
    AVCodecID codec,
    TranscodeMethod method,
    Qn::StreamQuality quality,
    const QSize& resolution,
    int bitrate,
    QnCodecParams::Value params )
{
    Q_UNUSED(params)
    if (bitrate == -1) {
        //bitrate = resolution.width() * resolution.height() * 5;
        bitrate = suggestMediaStreamParams( codec, resolution, quality, &params );
    }
    QnFfmpegVideoTranscoder* ffmpegTranscoder;
    m_videoCodec = codec;
    switch (method)
    {
        case TM_DirectStreamCopy:
            m_vTranscoder.reset();
            break;
        case TM_FfmpegTranscode:
        {
            ffmpegTranscoder = new QnFfmpegVideoTranscoder(codec);

            ffmpegTranscoder->setResolution(resolution);
            ffmpegTranscoder->setBitrate(bitrate);
            ffmpegTranscoder->setParams(params);
            ffmpegTranscoder->setQuality(quality);
            ffmpegTranscoder->setUseRealTimeOptimization(m_useRealTimeOptimization);
            m_extraTranscodeParams.setCodec(codec);
            ffmpegTranscoder->setFilterList(m_extraTranscodeParams.createFilterChain(resolution));

            if (codec != AV_CODEC_ID_H263P && codec != AV_CODEC_ID_MJPEG) {
                // H263P and MJPEG codecs have bug for multi thread encoding in current ffmpeg version
                bool isAtom = getCPUString().toLower().contains(QLatin1String("atom"));
                if (isAtom || resolution.height() >= 1080)
                    ffmpegTranscoder->setMTMode(true);
            }
            m_vTranscoder = QSharedPointer<QnFfmpegVideoTranscoder>(ffmpegTranscoder);
            break;
        }
            /*
        case TM_QuickSyncTranscode:
            m_vTranscoder = QnVideoTranscoderPtr(new QnQuickSyncTranscoder(codec));
            break;
            */
        case TM_OpenCLTranscode:
            m_lastErrMessage = tr("OpenCL transcoding is not implemented.");
            return OperationResult::Error;
        default:
            m_lastErrMessage = tr("Unknown transcoding method.");
            return OperationResult::Error;
    }
    return OperationResult::Success;
}

void QnTranscoder::setUseRealTimeOptimization(bool value)
{
    m_useRealTimeOptimization = value;
    if (m_vTranscoder)
        m_vTranscoder->setUseRealTimeOptimization(value);
}

QnTranscoder::OperationResult QnTranscoder::setAudioCodec(AVCodecID codec, TranscodeMethod method)
{
    m_audioCodec = codec;
    switch (method)
    {
        case TM_DirectStreamCopy:
            m_aTranscoder = QnAudioTranscoderPtr();
            break;
        case TM_FfmpegTranscode:
            m_aTranscoder = QnAudioTranscoderPtr(new QnFfmpegAudioTranscoder(codec));
            break;
            /*
        case TM_QuickSyncTranscode:
            m_vTranscoder = QnVideoTranscoderPtr(new QnQuickSyncTranscoder(codec));
            break;
            */
        case TM_OpenCLTranscode:
            m_lastErrMessage = tr("OpenCLTranscode is not implemented.");
            break;
        default:
            m_lastErrMessage = tr("Unknown transcode method");
            break;
    }
    return m_lastErrMessage.isEmpty()
        ? OperationResult::Success
        : OperationResult::Error;
}

int QnTranscoder::transcodePacket(const QnConstAbstractMediaDataPtr& media, QnByteArray* const result)
{
    m_internalBuffer.clear();
    m_outputPacketSize.clear();
    if (media->dataType == QnAbstractMediaData::EMPTY_DATA)
    {
        if (++m_eofCounter >= 3)
            return -8; // EOF reached
        else
            return 0;
    }
    else if (media->dataType != QnAbstractMediaData::VIDEO && media->dataType != QnAbstractMediaData::AUDIO)
        return 0; // transcode only audio and video, skip packet

    m_eofCounter = 0;

    if (m_firstTime == AV_NOPTS_VALUE)
        m_firstTime = media->timestamp;

    bool doTranscoding = true;
    static const size_t kMaxDelayedQueueSize = 60;

    if (!m_initialized)
    {
        if (media->dataType == QnAbstractMediaData::VIDEO)
            m_delayedVideoQueue << std::dynamic_pointer_cast<const QnCompressedVideoData> (media);
        else
            m_delayedAudioQueue << std::dynamic_pointer_cast<const QnCompressedAudioData> (media);
        doTranscoding = false;
        if (m_videoCodec != AV_CODEC_ID_NONE && m_delayedVideoQueue.isEmpty()
            && m_delayedAudioQueue.size() < (int)kMaxDelayedQueueSize)
        {
            return 0; // not ready to init
        }
        if (m_audioCodec != AV_CODEC_ID_NONE && m_delayedAudioQueue.isEmpty()
            && m_delayedVideoQueue.size() < (int)kMaxDelayedQueueSize)
        {
            return 0; // not ready to init
        }
        int rez = open(m_delayedVideoQueue.isEmpty() ? QnConstCompressedVideoDataPtr() : m_delayedVideoQueue.first(),
                       m_delayedAudioQueue.isEmpty() ? QnConstCompressedAudioDataPtr() : m_delayedAudioQueue.first());
        if (rez != 0)
            return rez;
    }

    if ((media->dataType == QnAbstractMediaData::AUDIO && !m_initializedAudio) ||
        (media->dataType == QnAbstractMediaData::VIDEO && !m_initializedVideo))
    {
        return OperationResult::Success;
    }

    if( result )
        result->clear();
    int errCode = OperationResult::Success;
    while (!m_delayedVideoQueue.isEmpty()) {
        errCode = transcodePacketInternal(m_delayedVideoQueue.dequeue(), result);
        if (errCode != 0)
            return errCode;
    }
    while (!m_delayedAudioQueue.isEmpty()) {
        errCode = transcodePacketInternal(m_delayedAudioQueue.dequeue(), result);
        if (errCode != 0)
            return errCode;
    }

    if (doTranscoding) {
        errCode = transcodePacketInternal(media, result);
        if (errCode != 0)
            return errCode;
    }

    if( result )
        result->write(m_internalBuffer.data(), m_internalBuffer.size());

    return OperationResult::Success;
}

int QnTranscoder::finalize(QnByteArray* const result)
{
    if (!m_initialized)
        return OperationResult::Success;

    m_internalBuffer.clear();
    finalizeInternal(result);
    if (result)
        result->write(m_internalBuffer.data(), m_internalBuffer.size());
    return OperationResult::Success;
}

bool QnTranscoder::addTag( const QString& /*name*/, const QString& /*value*/ )
{
    return false;
}

QString QnTranscoder::getLastErrorMessage() const
{
    return m_lastErrMessage;
}

int QnTranscoder::writeBuffer(const char* data, int size)
{
    m_internalBuffer.write(data,size);
    if (m_packetizedMode)
        m_outputPacketSize << size;
    return size;
}

void QnTranscoder::setPacketizedMode(bool value)
{
    m_packetizedMode = value;
}

const QVector<int>& QnTranscoder::getPacketsSize()
{
    return m_outputPacketSize;
}

void QnTranscoder::setExtraTranscodeParams(const QnImageFilterHelper& extraParams)
{
    m_extraTranscodeParams = extraParams;
}

#endif // ENABLE_DATA_PROVIDERS

