#include "transcoder.h"

#include <utils/math/math.h>
#include <core/resource/media_resource.h>

#include "ffmpeg_transcoder.h"
#include "ffmpeg_video_transcoder.h"
#include "ffmpeg_audio_transcoder.h"

#include "filters/abstract_image_filter.h"
#include "filters/tiled_image_filter.h"
#include "filters/scale_image_filter.h"
#include "filters/rotate_image_filter.h"

// ---------------------------- QnCodecTranscoder ------------------
QnCodecTranscoder::QnCodecTranscoder(CodecID codecId)
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
    int left = qPower2Floor((unsigned) srcRect.left(), 16);
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

QnVideoTranscoder::QnVideoTranscoder(CodecID codecId):
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
        result = filter->updateImage(result);
    return result;
}

QSize QnVideoTranscoder::getResolution() const
{
    return m_resolution;
}

bool QnVideoTranscoder::open(const QnConstCompressedVideoDataPtr& video)
{
    CLFFmpegVideoDecoder decoder(video->compressionType, video, false);
    QSharedPointer<CLVideoDecoderOutput> decodedVideoFrame( new CLVideoDecoderOutput() );
    decoder.decode(video, &decodedVideoFrame);
    bool lineAmountSpecified = false;
    if (m_resolution.width() == 0 && m_resolution.height() > 0)
    {
        m_resolution.setHeight(qPower2Ceil((unsigned) m_resolution.height(),8)); // round resolution height
        m_resolution.setHeight(qMin(decoder.getContext()->height, m_resolution.height())); // strict to source frame height

        float ar = decoder.getContext()->width / (float) decoder.getContext()->height;
        m_resolution.setWidth(m_resolution.height() * ar);
        m_resolution.setWidth(qPower2Ceil((unsigned) m_resolution.width(),16)); // round resolution width
        m_resolution.setWidth(qMin(decoder.getContext()->width, m_resolution.width())); // strict to source frame width
        lineAmountSpecified = true;
    }
    else if ((m_resolution.width() == 0 && m_resolution.height() == 0) || m_resolution.isEmpty()) {
        m_resolution = QSize(decoder.getContext()->width, decoder.getContext()->height);
    }

    for(auto filter: m_filters)
        setResolution(filter->updatedResolution(getResolution()));

    return true;
}

// ---------------------- QnTranscoder -------------------------


QnTranscoder::QnTranscoder():
    m_videoCodec(CODEC_ID_NONE),
    m_audioCodec(CODEC_ID_NONE),
    m_videoStreamCopy(false),
    m_audioStreamCopy(false),
    m_internalBuffer(CL_MEDIA_ALIGNMENT, 1024*1024),
    m_firstTime(AV_NOPTS_VALUE),
    m_initialized(false),
    m_eofCounter(0),
    m_packetizedMode(false)
{
    QThread::currentThread()->setPriority(QThread::LowPriority); 
}

QnTranscoder::~QnTranscoder()
{

}

int QnTranscoder::suggestMediaStreamParams(
    CodecID codec,
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
        case Qn::QualityLow:
            hiEnd = 1024 + 512;
        case Qn::QualityNormal:
            hiEnd = 1024*2;
        case Qn::QualityHigh:
            hiEnd = 1024*3;
        case Qn::QualityHighest:
        default:
            hiEnd = 1024*5;
    }

    float resolutionFactor = resolution.width()*resolution.height()/1920.0/1080;
    resolutionFactor = pow(resolutionFactor, (float)0.63); // 256kbps for 320x240 for Mq quality

    int result = hiEnd * resolutionFactor;

    if( codec == CODEC_ID_MJPEG )
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
    CodecID codec,
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
            m_vTranscoder = QnVideoTranscoderPtr();
            break;
        case TM_FfmpegTranscode:
        {
            ffmpegTranscoder = new QnFfmpegVideoTranscoder(codec);

            ffmpegTranscoder->setResolution(resolution);
            ffmpegTranscoder->setBitrate(bitrate);
            ffmpegTranscoder->setParams(params);
            ffmpegTranscoder->setQuality(quality);
            ffmpegTranscoder->setFilterList(m_extraTranscodeParams.createFilterChain(resolution));

            bool isAtom = getCPUString().toLower().contains(QLatin1String("atom"));
            if (isAtom || resolution.height() >= 1080)
                ffmpegTranscoder->setMTMode(true);
            m_vTranscoder = QnVideoTranscoderPtr(ffmpegTranscoder);
            break;
        }
            /*
        case TM_QuickSyncTranscode:
            m_vTranscoder = QnVideoTranscoderPtr(new QnQuickSyncTranscoder(codec));
            break;
            */
        case TM_OpenCLTranscode:
            m_lastErrMessage = tr("OpenCL transcoding is not implemented.");
            return -1;
        default:
            m_lastErrMessage = tr("Unknown transcoding method.");
            return -1;
    }
    return 0;
}

int QnTranscoder::setAudioCodec(CodecID codec, TranscodeMethod method)
{
    Q_UNUSED(method);
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
            m_lastErrMessage = tr("OpenCLTranscode is not implemented");
            break;
        default:
            m_lastErrMessage = tr("Unknown Transcode Method");
            break;
    }
    return m_lastErrMessage.isEmpty() ? 0 : 1;
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

    if ((quint64)m_firstTime == AV_NOPTS_VALUE)
        m_firstTime = media->timestamp;

    bool doTranscoding = true;
    if (!m_initialized)
    {
        if (media->dataType == QnAbstractMediaData::VIDEO)
            m_delayedVideoQueue << qSharedPointerDynamicCast<const QnCompressedVideoData> (media);
        else
            m_delayedAudioQueue << qSharedPointerDynamicCast<const QnCompressedAudioData> (media);
        doTranscoding = false;
        if (m_videoCodec != CODEC_ID_NONE && m_delayedVideoQueue.isEmpty())
            return 0; // not ready to init
        if (m_audioCodec != CODEC_ID_NONE && m_delayedAudioQueue.isEmpty())
            return 0; // not ready to init
        int rez = open(m_delayedVideoQueue.isEmpty() ? QnConstCompressedVideoDataPtr() : m_delayedVideoQueue.first(), 
                       m_delayedAudioQueue.isEmpty() ? QnConstCompressedAudioDataPtr() : m_delayedAudioQueue.first());
        if (rez != 0)
            return rez;
        m_initialized = true;
    }

    if( result )
        result->clear();
    int errCode = 0;
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
    
    return 0;
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
