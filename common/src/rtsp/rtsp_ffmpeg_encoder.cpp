#include "rtsp_ffmpeg_encoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include "core/datapacket/video_data_packet.h"

#include "network/ffmpeg_sdp.h"
#include "network/rtpsession.h"
#include "network/rtp_stream_parser.h"

#include "utils/common/util.h"
#include "utils/media/ffmpeg_helper.h"
#include "utils/network/socket.h"

QnRtspFfmpegEncoder::QnRtspFfmpegEncoder(): 
    m_gotLivePacket(false),
    m_curDataBuffer(0),
    m_liveMarker(0),
    m_additionFlags(0),
    m_eofReached(false),
    m_isLastDataContext(false)
{

}    

void QnRtspFfmpegEncoder::init()
{ 
    m_contextSent.reset();
    m_gotLivePacket = false;
    m_eofReached = false;
}

QnMediaContextPtr QnRtspFfmpegEncoder::getGeneratedContext(CodecID compressionType)
{
    QMap<CodecID, QnMediaContextPtr>::iterator itr = m_generatedContexts.find(compressionType);
    QnMediaContextPtr result;
    if (itr != m_generatedContexts.end())
    {
        result = itr.value();
    }
    else
    {
        result = QnMediaContextPtr(new QnMediaContext(compressionType));
        m_generatedContexts.insert(compressionType, result);
    }

    assert(result);
    return result;
}

void QnRtspFfmpegEncoder::setDataPacket(QnConstAbstractMediaDataPtr media)
{
    m_media = media;
    m_curDataBuffer = media->data();
    m_codecCtxData.clear();
    if (m_media->flags & QnAbstractMediaData::MediaFlags_AfterEOF)
        m_contextSent.reset();

    QnConstMetaDataV1Ptr metadata = std::dynamic_pointer_cast<const QnMetaDataV1>(m_media);
    if (!metadata && m_media->compressionType)
    {
        QnMediaContextPtr currentContext = m_media->context;
        if (!currentContext)
            currentContext = getGeneratedContext(m_media->compressionType);
        assert(currentContext);
        //int rtpHeaderSize = 4 + RtpHeader::RTP_HEADER_SIZE;
        if (!m_contextSent || !m_contextSent->isSimilarTo(currentContext))
        {
            m_contextSent = currentContext;
            m_codecCtxData = currentContext->serialize();
        }
    }
    m_eofReached = false;
}

bool QnRtspFfmpegEncoder::getNextPacket(QnByteArray& sendBuffer)
{
    sendBuffer.resize(sendBuffer.size() + RtpHeader::RTP_HEADER_SIZE); // reserve space for RTP header

    if (!m_codecCtxData.isEmpty())
    {
        Q_ASSERT(!m_codecCtxData.isEmpty());
        sendBuffer.write(m_codecCtxData);
        m_codecCtxData.clear();
        m_isLastDataContext = true;
        return true;
    }
    m_isLastDataContext = false;

    quint16 flags = m_media->flags;
    flags |= m_additionFlags;

    int cseq = m_media->opaque;

    bool isLive = m_media->flags & QnAbstractMediaData::MediaFlags_LIVE;
    if (isLive) {
        cseq = m_liveMarker;
        if (!m_gotLivePacket)
            flags |= QnAbstractMediaData::MediaFlags_BOF;
        m_gotLivePacket = true;
    }

    // one video channel may has several subchannels (video combined with frames from difference codecContext)
    // max amount of subchannels is MAX_CONTEXTS_AT_VIDEO. Each channel used 2 ssrc: for data and for CodecContext

    const char* dataEnd = m_media->data() + m_media->dataSize();
    int dataRest = dataEnd - m_curDataBuffer;
    if (m_eofReached)
        return false; // no more data to send
    int sendLen = qMin(MAX_RTSP_DATA_LEN - RTSP_FFMPEG_MAX_HEADER_SIZE, dataRest);

    if (m_curDataBuffer == m_media->data())
    {
        // send data with RTP headers
        const QnCompressedVideoData* video = dynamic_cast<const QnCompressedVideoData*>(m_media.get());
        const QnMetaDataV1* metadata = dynamic_cast<const QnMetaDataV1*>(m_media.get());
#if 0
        int ffHeaderSize = RTSP_FFMPEG_GENERIC_HEADER_SIZE;
        if (video)
            ffHeaderSize += RTSP_FFMPEG_VIDEO_HEADER_SIZE;
        else if (metadata)
            ffHeaderSize += RTSP_FFMPEG_METADATA_HEADER_SIZE;
#endif
        //m_mutex.unlock();

        //buildRtspTcpHeader(rtpTcpChannel, ssrc, sendLen + ffHeaderSize, sendLen >= dataRest ? 1 : 0, media->timestamp, RTP_FFMPEG_GENERIC_CODE);
        //QnMutexLocker lock( &m_owner->getSockMutex() );
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
            quint32 videoHeader = htonl(video->dataSize() & 0x00ffffff);
            sendBuffer.write(((const char*) &videoHeader)+1, 3);
        }
        else if (metadata) {
            quint32 metadataHeader = htonl(metadata->m_duration/1000);
            sendBuffer.write((const char*) &metadataHeader, 4);
        }
    }

    sendBuffer.write(m_curDataBuffer, sendLen);
    m_curDataBuffer += sendLen;

    m_eofReached = m_curDataBuffer == dataEnd;

    return true;
}

quint32 QnRtspFfmpegEncoder::getSSRC()
{
    return BASIC_FFMPEG_SSRC + (m_isLastDataContext ? 1 : 0);
}

bool QnRtspFfmpegEncoder::getRtpMarker()
{
    int dataRest = m_media->data() + static_cast<int>(m_media->dataSize()) - m_curDataBuffer;
    return m_isLastDataContext || dataRest == 0;
}

quint32 QnRtspFfmpegEncoder::getFrequency()
{
    return 1000000;
}

quint8 QnRtspFfmpegEncoder::getPayloadtype()
{
    return RTP_FFMPEG_GENERIC_CODE;
}

QByteArray QnRtspFfmpegEncoder::getAdditionSDP( const std::map<QString, QString>& /*streamParams*/ )
{
    if (!m_codecCtxData.isEmpty()) {
        QString result(lit("a=fmtp:%1 config=%2\r\n"));
        return result.arg((int)getPayloadtype()).arg(QLatin1String(m_codecCtxData.toBase64())).toLatin1();
    }
    else {
        return QByteArray();
    }
}

QString QnRtspFfmpegEncoder::getName()
{
    return RTP_FFMPEG_GENERIC_STR;
}

void QnRtspFfmpegEncoder::setLiveMarker(int value)
{
    m_liveMarker = value;
}

void QnRtspFfmpegEncoder::setAdditionFlags(quint16 value)
{
    m_additionFlags = value;
}

void QnRtspFfmpegEncoder::setCodecContext(const QnConstMediaContextPtr& context)
{
    if (context)
        m_codecCtxData = context->serialize();
}

#endif // ENABLE_DATA_PROVIDERS
