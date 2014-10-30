#include "transcoder.h"

#include <utils/math/math.h>
#include <core/resource/media_resource.h>

#include "filters/abstract_image_filter.h"

#include "ffmpeg_transcoder.h"
#include "ffmpeg_video_transcoder.h"
#include "ffmpeg_audio_transcoder.h"

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

void QnCodecTranscoder::setSrcRect(const QRectF& srcRect)
{
    m_srcRectF = srcRect;
}

QRect QnCodecTranscoder::roundRect(const QRect& srcRect) const
{
    int left = qPower2Floor((unsigned) srcRect.left(), 16);
    int top = qPower2Floor((unsigned) srcRect.top(), 2);

    int width = qPower2Ceil((unsigned) srcRect.width(), 16);
    int height = qPower2Ceil((unsigned) srcRect.height(), 2);

    return QRect(left, top, width, height);
}

// --------------------------- QnVideoTranscoder -----------------

QnVideoTranscoder::QnVideoTranscoder(CodecID codecId):
    QnCodecTranscoder(codecId),
    m_layout(0)
{

}

QnVideoTranscoder::~QnVideoTranscoder()
{
    for(QnAbstractImageFilter* filter: m_filters)
        delete filter;
}

void QnVideoTranscoder::setVideoLayout(QnConstResourceVideoLayoutPtr layout)
{
    m_layout = layout;
}

void QnVideoTranscoder::setResolution(const QSize& value)
{
    m_resolution = value;
}

void QnVideoTranscoder::addFilter(QnAbstractImageFilter* filter)
{
    m_filters << filter;
}

CLVideoDecoderOutputPtr QnVideoTranscoder::processFilterChain(const CLVideoDecoderOutputPtr& decodedFrame, const QRectF& updateRect, qreal ar)
{
    if (m_filters.isEmpty())
        return decodedFrame;
    CLVideoDecoderOutputPtr result;
    for(QnAbstractImageFilter* filter: m_filters)
        result = filter->updateImage(decodedFrame, updateRect, ar);
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

    int width = m_resolution.width();
    int height = m_resolution.height();
    if (m_layout) {
        if (lineAmountSpecified) {
            int maxDimension = qMax(m_layout->size().width(), m_layout->size().height());
            width /= maxDimension;
            height /= maxDimension;
        }
        width = qPower2Ceil((unsigned) width , WIDTH_ALIGN);
        height = qPower2Ceil((unsigned) height , HEIGHT_ALIGN);

        width *= m_layout->size().width();
        height *= m_layout->size().height();
    }

    if (!m_srcRectF.isEmpty())
    {
        // round srcRect
        int srcLeft = qPower2Floor(unsigned(m_srcRectF.left() * width), WIDTH_ALIGN);
        int srcTop = qPower2Floor(unsigned(m_srcRectF.top() * height), HEIGHT_ALIGN);
        int srcWidth = qPower2Ceil(unsigned(m_srcRectF.width() * width), WIDTH_ALIGN);
        int srcHeight = qPower2Ceil(unsigned(m_srcRectF.height() * height), HEIGHT_ALIGN);

        m_srcRectF = QRectF(srcLeft / (qreal) width, srcTop / (qreal) height, 
                            srcWidth / (qreal) width, srcHeight / (qreal) height);

        width  = srcWidth;
        height = srcHeight;
    }

    m_resolution.setWidth(qPower2Ceil((unsigned)width, WIDTH_ALIGN));
    m_resolution.setHeight(qPower2Ceil((unsigned)height, HEIGHT_ALIGN));

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
    m_vLayout(0),
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

void QnTranscoder::setVideoLayout(QnConstResourceVideoLayoutPtr layout)
{
    m_vLayout = layout;
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
            ffmpegTranscoder->setVideoLayout(m_vLayout);
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
    if (m_vTranscoder)
    {
        m_vTranscoder->setResolution(resolution);
        m_vTranscoder->setBitrate(bitrate);
        m_vTranscoder->setParams(params);
        m_vTranscoder->setQuality(quality);
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
