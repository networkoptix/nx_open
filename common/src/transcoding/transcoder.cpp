#include "transcoder.h"
#include "ffmpeg_transcoder.h"
#include "core/resource/media_resource.h"
#include "ffmpeg_video_transcoder.h"
#include "ffmpeg_audio_transcoder.h"

// ---------------------------- QnCodecTranscoder ------------------
QnCodecTranscoder::QnCodecTranscoder(CodecID codecId):
    m_bitrate(-1)
{
    m_codecId = codecId;
}


void QnCodecTranscoder::setParams(const Params& params)
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

// --------------------------- QnVideoTranscoder -----------------

QnVideoTranscoder::QnVideoTranscoder(CodecID codecId):
    QnCodecTranscoder(codecId)
{

}


void QnVideoTranscoder::setResolution(const QSize& value)
{
    m_resolution = value;
}

QSize QnVideoTranscoder::getResolution() const
{
    return m_resolution;
}

bool QnVideoTranscoder::open(QnCompressedVideoDataPtr video)
{
    CLFFmpegVideoDecoder decoder(video->compressionType, video, false);
    QSharedPointer<CLVideoDecoderOutput> decodedVideoFrame( new CLVideoDecoderOutput() );
    decoder.decode(video, &decodedVideoFrame);
    if (m_resolution.width() == 0 && m_resolution.height() > 0)
    {
        m_resolution.setHeight(qPower2Ceil((unsigned) m_resolution.height(),16)); // round resolution height
        m_resolution.setHeight(qMin(decoder.getContext()->height, m_resolution.height())); // strict to source frame height

        float ar = decoder.getContext()->width / (float) decoder.getContext()->height;
        m_resolution.setWidth(m_resolution.height() * ar);
        m_resolution.setWidth(qPower2Ceil((unsigned) m_resolution.width(),16)); // round resolution width
        m_resolution.setWidth(qMin(decoder.getContext()->width, m_resolution.width())); // strict to source frame width
    }
    else if ((m_resolution.width() == 0 && m_resolution.height() == 0) || m_resolution.isEmpty())
        m_resolution = QSize(decoder.getContext()->width, decoder.getContext()->height);

    return true;
}

// ---------------------- QnTranscoder -------------------------

QnTranscoder::QnTranscoder():
    m_videoCodec(CODEC_ID_NONE),
    m_audioCodec(CODEC_ID_NONE),
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

int QnTranscoder::suggestBitrate(QSize resolution, QnStreamQuality quality)
{
    // I assume for a QnQualityHighest quality 30 fps for 1080 we need 10 mbps
    // I assume for a QnQualityLowest quality 30 fps for 1080 we need 1 mbps

    if (resolution.width() == 0)
        resolution.setWidth(resolution.height()*4/3);

    int hiEnd;
    switch(quality)
    {
        case QnQualityLowest:
            hiEnd = 1024;
        case QnQualityLow:
            hiEnd = 1024 + 512;
        case QnQualityNormal:
            hiEnd = 1024*2;
        case QnQualityHigh:
            hiEnd = 1024*3;
        case QnQualityHighest:
        default:
            hiEnd = 1024*5;
    }

    float resolutionFactor = resolution.width()*resolution.height()/1920.0/1080;
    resolutionFactor = pow(resolutionFactor, (float)0.63); // 256kbps for 320x240 for Mq quality

    int result = hiEnd * resolutionFactor;

    return qMax(128,result)*1024;
}

int QnTranscoder::setVideoCodec(CodecID codec, TranscodeMethod method, const QSize& resolution, int bitrate, const QnCodecTranscoder::Params& params)
{
    Q_UNUSED(params)
    if (bitrate == -1) {
        //bitrate = resolution.width() * resolution.height() * 5;
        bitrate = suggestBitrate(resolution);
    }

    m_videoCodec = codec;
    switch (method)
    {
        case TM_DirectStreamCopy:
            m_vTranscoder = QnVideoTranscoderPtr();
            break;
        case TM_FfmpegTranscode:
            m_vTranscoder = QnVideoTranscoderPtr(new QnFfmpegVideoTranscoder(codec));
            break;
            /*
        case TM_QuickSyncTranscode:
            m_vTranscoder = QnVideoTranscoderPtr(new QnQuickSyncTranscoder(codec));
            break;
            */
        case TM_OpenCLTranscode:
            m_lastErrMessage = tr("OpenCLTranscode is not implemented");
            return -1;
        default:
            //TODO: #vasilenko Check 'Value not handled in switch' case.
            m_lastErrMessage = tr("Unknown Transcode Method");
            return -1;
    }
    if (m_vTranscoder)
    {
        m_vTranscoder->setResolution(resolution);
        m_vTranscoder->setBitrate(bitrate);
    }
    return 0;
}

bool QnTranscoder::setAudioCodec(CodecID codec, TranscodeMethod method)
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
            return -1;
        default:
            //TODO: #vasilenko Check 'Value not handled in switch' case.
            m_lastErrMessage = tr("Unknown Transcode Method");
            return -1;
    }
    return m_lastErrMessage.isEmpty();
}

int QnTranscoder::transcodePacket(QnAbstractMediaDataPtr media, QnByteArray& result)
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

    if (!m_initialized)
    {
        if (media->dataType == QnAbstractMediaData::VIDEO)
            m_delayedVideoQueue << qSharedPointerDynamicCast<QnCompressedVideoData> (media);
        else
            m_delayedAudioQueue << qSharedPointerDynamicCast<QnCompressedAudioData> (media);
        media.clear();
        if (m_videoCodec != CODEC_ID_NONE && m_delayedVideoQueue.isEmpty())
            return 0; // not ready to init
        if (m_audioCodec != CODEC_ID_NONE && m_delayedAudioQueue.isEmpty())
            return 0; // not ready to init
        int rez = open(m_delayedVideoQueue.isEmpty() ? QnCompressedVideoDataPtr() : m_delayedVideoQueue.first(), 
                       m_delayedAudioQueue.isEmpty() ? QnCompressedAudioDataPtr() : m_delayedAudioQueue.first());
        if (rez != 0)
            return rez;
        m_initialized = true;
    }
    result.clear();
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
    if (media) {
        errCode = transcodePacketInternal(media, result);
        if (errCode != 0)
            return errCode;
    }
    
    result.write(m_internalBuffer.data(), m_internalBuffer.size());
    
    return 0;
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
