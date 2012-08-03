#include "rtsp_ffmpeg_encoder.h"
#include "utils/network/ffmpeg_sdp.h"
#include "utils/network/socket.h"
#include "utils/media/ffmpeg_helper.h"
#include "rtsp_data_consumer.h"
#include "utils/common/util.h"

QnRtspFfmpegEncoder::QnRtspFfmpegEncoder(): 
    m_gotLivePacket(false),
    m_curDataBuffer(0)
{

}    

void QnRtspFfmpegEncoder::init()
{ 
    m_ctxSended.clear();
    m_gotLivePacket = false;
}

QnMediaContextPtr QnRtspFfmpegEncoder::getGeneratedContext(CodecID compressionType)
{
    QMap<CodecID, QnMediaContextPtr>::iterator itr = m_generatedContext.find(compressionType);
    if (itr != m_generatedContext.end())
        return itr.value();
    QnMediaContextPtr result(new QnMediaContext(compressionType));
    AVCodecContext* ctx = result->ctx();
    m_generatedContext.insert(compressionType, result);
    return result;
}

void QnRtspFfmpegEncoder::setDataPacket(QnAbstractMediaDataPtr media)
{
    m_media = media;
    m_curDataBuffer = media->data.data();
    m_codecCtxData.clear();

    QnMetaDataV1Ptr metadata = qSharedPointerDynamicCast<QnMetaDataV1>(m_media);
    int subChannelNumber = m_media->subChannelNumber;
    if (!metadata && m_media->compressionType)
    {
        QList<QnMediaContextPtr>& ctxData = m_ctxSended[m_media->channelNumber];
        while (ctxData.size() <= subChannelNumber)
            ctxData << QnMediaContextPtr(0);

        QnMediaContextPtr currentContext = m_media->context;
        if (currentContext == 0)
            currentContext = getGeneratedContext(m_media->compressionType);
        //int rtpHeaderSize = 4 + RtpHeader::RTP_HEADER_SIZE;
        if (ctxData[subChannelNumber] == 0 || !ctxData[subChannelNumber]->equalTo(currentContext.data()))
        {
            ctxData[subChannelNumber] = currentContext;
            QnFfmpegHelper::serializeCodecContext(currentContext->ctx(), &m_codecCtxData);
        }
    }
}

bool QnRtspFfmpegEncoder::getNextPacket(QnByteArray& sendBuffer)
{
    if (!m_codecCtxData.isEmpty())
    {
        Q_ASSERT(!m_codecCtxData.isEmpty());
        sendBuffer.write(m_codecCtxData);
        m_codecCtxData.clear();
        return true;
    }

    quint16 flags = m_media->flags;
    int cseq = m_media->opaque;

    bool isLive = m_media->flags & QnAbstractMediaData::MediaFlags_LIVE;
    if (isLive) {
        if (!m_gotLivePacket)
            flags |= QnAbstractMediaData::MediaFlags_BOF;
        m_gotLivePacket = true;
    }
    if (flags & QnAbstractMediaData::MediaFlags_AfterEOF)
        m_ctxSended.clear();


    // one video channel may has several subchannels (video combined with frames from difference codecContext)
    // max amount of subchannels is MAX_CONTEXTS_AT_VIDEO. Each channel used 2 ssrc: for data and for CodecContext

    int dataRest = m_media->data.data() + m_media->data.size() - m_curDataBuffer;
    if (dataRest == 0)
        return false; // no more data to send
    int sendLen = qMin(MAX_RTSP_DATA_LEN - RTSP_FFMPEG_MAX_HEADER_SIZE, dataRest);

    if (m_curDataBuffer == m_media->data.data())
    {
        // send data with RTP headers
        QnCompressedVideoData *video = m_media.dynamicCast<QnCompressedVideoData>().data();
        QnMetaDataV1Ptr metadata = qSharedPointerDynamicCast<QnMetaDataV1>(m_media);
        int ffHeaderSize = RTSP_FFMPEG_GENERIC_HEADER_SIZE;
        if (video)
            ffHeaderSize += RTSP_FFMPEG_VIDEO_HEADER_SIZE;
        else if (metadata)
            ffHeaderSize += RTSP_FFMPEG_METADATA_HEADER_SIZE;

        //m_mutex.unlock();

        //buildRtspTcpHeader(rtpTcpChannel, ssrc, sendLen + ffHeaderSize, sendLen >= dataRest ? 1 : 0, media->timestamp, RTP_FFMPEG_GENERIC_CODE);
        //QMutexLocker lock(&m_owner->getSockMutex());
        //sendBuffer->write(m_rtspTcpHeader, sizeof(m_rtspTcpHeader));
        quint8 packetType = m_media->dataType;
        sendBuffer.write((const char*) &packetType, 1);
        quint32 timestampHigh = htonl(m_media->timestamp >> 32);
        sendBuffer.write((const char*) &timestampHigh, 4);
        quint8 cseq8 = cseq;
        sendBuffer.write((const char*) &cseq8, 1);
        flags = htons(flags);
        sendBuffer.write((const char*) &flags, 2);
        if (video) 
        {
            quint32 videoHeader = htonl(video->data.size() & 0x00ffffff);
            sendBuffer.write(((const char*) &videoHeader)+1, 3);
        }
        else if (metadata) {
            quint32 metadataHeader = htonl(metadata->m_duration/1000);
            sendBuffer.write((const char*) &metadataHeader, 4);
        }
    }

    sendBuffer.write(m_curDataBuffer, sendLen);
    m_curDataBuffer += sendLen;

    return true;
}

quint32 QnRtspFfmpegEncoder::getSSRC()
{
    quint32 ssrc = BASIC_FFMPEG_SSRC + m_media->channelNumber * MAX_CONTEXTS_AT_VIDEO*2;
    ssrc += m_media->subChannelNumber*2;
    return ssrc + (m_codecCtxData.isEmpty() ? 0 : 1);
}

bool QnRtspFfmpegEncoder::getRtpMarker()
{
    if (!m_codecCtxData.isEmpty())
        return true;

    int dataRest = m_media->data.data() + m_media->data.size() - m_curDataBuffer;
    int sendLen = qMin(MAX_RTSP_DATA_LEN - RTSP_FFMPEG_MAX_HEADER_SIZE, dataRest);

    return sendLen >= dataRest ? 1 : 0;
}

quint32 QnRtspFfmpegEncoder::getFrequency()
{
    return 1000000;
}

quint8 QnRtspFfmpegEncoder::getPayloadtype()
{
    return RTP_FFMPEG_GENERIC_CODE;
}

QByteArray QnRtspFfmpegEncoder::getAdditionSDP()
{
    return QByteArray();
}

QString QnRtspFfmpegEncoder::getName()
{
    return RTP_FFMPEG_GENERIC_STR;
}
