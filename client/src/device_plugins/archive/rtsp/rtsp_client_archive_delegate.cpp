#include "rtsp_client_archive_delegate.h"
#include "core/datapacket/mediadatapacket.h"
#include "core/resourcemanagment/resource_pool.h"
#include "utils/network/rtp_stream_parser.h"
#include "libavcodec/avcodec.h"
#include "utils/media/ffmpeg_helper.h"
#include "utils/network/ffmpeg_sdp.h"
#include "utils/common/util.h"

static const int MAX_RTP_BUFFER_SIZE = 65535;

QnRtspClientArchiveDelegate::QnRtspClientArchiveDelegate():
    QnAbstractArchiveDelegate(),
    m_tcpMode(true),
    m_rtpData(0),
    m_position(DATETIME_NOW),
    m_opened(false)
{
    m_rtpDataBuffer = new quint8[MAX_RTP_BUFFER_SIZE];
    m_flags |= Flag_SlowSource;
    m_flags |= Flag_CanProcessNegativeSpeed;
}

QnRtspClientArchiveDelegate::~QnRtspClientArchiveDelegate()
{
    close();
    delete [] m_rtpDataBuffer;
}

bool QnRtspClientArchiveDelegate::open(QnResourcePtr resource)
{
    if (m_opened)
        return true;
    QnResourcePtr server = qnResPool->getResourceById(resource->getParentId());
    if (server == 0)
        return false;
    m_rtspSession.setTransport("TCP");

    QString url = server->getUrl() + QString('/');
    /*
    if (!resource->getId().isValid())
        url += resource->getUrl();
    else
        url += resource->getId().toString();
    */
    url += resource->getUrl();
    if (m_rtspSession.open(url)) 
    {
        m_rtpData = m_rtspSession.play(m_position);
        if (!m_rtpData)
            m_rtspSession.stop();
    }
    else {
        m_rtspSession.stop();
    }
    m_opened = m_rtspSession.isOpened();
    return m_opened;
}

void QnRtspClientArchiveDelegate::close()
{
    m_rtspSession.stop();
    m_rtpData = 0;
    deleteContexts();
    m_opened = false;
}

void QnRtspClientArchiveDelegate::deleteContexts()
{
    m_contextMap.clear();
}

qint64 QnRtspClientArchiveDelegate::startTime()
{
    return m_rtspSession.startTime();
}

qint64 QnRtspClientArchiveDelegate::endTime()
{
    return m_rtspSession.endTime();
}

QnAbstractMediaDataPtr QnRtspClientArchiveDelegate::getNextData()
{
    // sometime function may return zero packet if no data arrived
    QnAbstractMediaDataPtr result;
    while (!result)
    {
        if (!m_rtpData)
            return result;

        int rtpChannelNum = 0;
        int blockSize  = m_rtpData->read((char*)m_rtpDataBuffer, MAX_RTP_BUFFER_SIZE);
        if (blockSize < 0) {
            m_rtspSession.stop();
            return result; // reconnect
        }

#ifdef DEBUG_RTSP
        static QFile* binaryFile = 0;
        if (!binaryFile) {
            binaryFile = new QFile("c:/binary2.rtsp");
            binaryFile->open(QFile::WriteOnly);
        }
        binaryFile->write((const char*) m_rtpDataBuffer, blockSize);
        binaryFile->flush();

#endif

        quint8* data = m_rtpDataBuffer;
        if (m_tcpMode)
        {
            if (blockSize < 4) {
                qWarning() << Q_FUNC_INFO << __LINE__ << "strange RTP/TCP packet. len < 4. Ignored";
                return result;
            }
            rtpChannelNum = m_rtpDataBuffer[1];
            blockSize -= 4;
            data += 4;
        }
        else {
            rtpChannelNum = m_rtpData->getSocket()->getLocalPort();
        }
        const QString format = m_rtspSession.getTrackFormat(rtpChannelNum).toLower();
        if (format.isEmpty())
            qWarning() << Q_FUNC_INFO << __LINE__ << "RTP track" << rtpChannelNum << "not found";
        else if (format == QLatin1String("ffmpeg")) {
            result = qSharedPointerDynamicCast<QnAbstractMediaData>(processFFmpegRtpPayload(data, blockSize));
        }
        else
            qWarning() << Q_FUNC_INFO << __LINE__ << "Only FFMPEG payload format now implemeted. Ask developers to add '" << format << "' format";
    }
    
  
    return result;
}

qint64 QnRtspClientArchiveDelegate::seek(qint64 time)
{
    deleteContexts(); // context is going to create again on first data after SEEK, so ignore rest of data before seek
    m_rtspSession.sendPlay(time, m_rtspSession.getScale());
    return time;
}

QnVideoResourceLayout* QnRtspClientArchiveDelegate::getVideoLayout()
{
    // todo: implement me
    return &m_defaultVideoLayout;
}

QnResourceAudioLayout* QnRtspClientArchiveDelegate::getAudioLayout()
{
    // todo: implement me
    static QnEmptyAudioLayout emptyAudioLayout;
    return &emptyAudioLayout;
}

QnAbstractDataPacketPtr QnRtspClientArchiveDelegate::processFFmpegRtpPayload(const quint8* data, int dataSize)
{
    QnAbstractDataPacketPtr result;
    if (dataSize < RtpHeader::RTP_HEADER_SIZE)
    {
        qWarning() << Q_FUNC_INFO << __LINE__ << "strange RTP packet. len < RTP header size. Ignored";
        return result;
    }

    RtpHeader* rtpHeader = (RtpHeader*) data;
    const quint8* payload = data + RtpHeader::RTP_HEADER_SIZE;
    dataSize -= RtpHeader::RTP_HEADER_SIZE;
    quint32 ssrc = ntohl(rtpHeader->ssrc);
    const bool isCodecContext = ssrc & 1; // odd numbers - codec context, even numbers - data
    if (isCodecContext)
    {
        if (!m_contextMap.contains(ssrc))
        {
            QnMediaContextPtr context(new QnMediaContext(payload, dataSize));
            if (context)
                m_contextMap.insert(ssrc, context);
        }
    }
    else
    {
        QMap<int, QnMediaContextPtr>::iterator itr = m_contextMap.find(ssrc + 1);
        QnMediaContextPtr context;
        if (itr != m_contextMap.end())
            context = itr.value();
        else 
            return result;

        if (rtpHeader->padding)
            dataSize -= ntohl(rtpHeader->padding);
        if (dataSize < 1)
            return result;

        QnAbstractDataPacketPtr nextPacket = m_nextDataPacket.value(ssrc);
        QnAbstractMediaDataPtr mediaPacket = qSharedPointerDynamicCast<QnAbstractMediaData>(nextPacket);
        if (!nextPacket)
        {
            if (dataSize < 4)
                return result;

            quint32 timestampHigh = ntohl(*(quint32*)payload);
            dataSize-=4;
            payload+=4; // deserialize timeStamp high part
            if (context->ctx()->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                if (dataSize < 1)
                    return result;

                quint32 videoHeader = ntohl(*(quint32*)payload);
                quint8 flags = videoHeader >> 24;
                quint32 fullPayloadLen = videoHeader & 0x00ffffff;
                dataSize-=4;
                payload+=4; // deserialize video flags
                QnCompressedVideoData *video = new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, fullPayloadLen, context);
                nextPacket = QnCompressedVideoDataPtr(video);
                video->flags = flags;
                if (context) 
                {
                    video->width = context->ctx()->coded_width;
                    video->height = context->ctx()->coded_height;
                }
            }
            else if (context->ctx()->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                QnCompressedAudioData *audio = new QnCompressedAudioData(CL_MEDIA_ALIGNMENT, dataSize); // , context
                audio->format.fromAvStream(context->ctx());
                nextPacket = QnCompressedAudioDataPtr(audio);
            }
            else
            {
                qWarning() << "Unsupported RTP media type " << context->ctx()->codec_type;
                return result;
            }
            m_nextDataPacket[ssrc] = nextPacket;
            mediaPacket = qSharedPointerDynamicCast<QnAbstractMediaData>(nextPacket);
            if (mediaPacket) {
                mediaPacket->compressionType = context->ctx()->codec_id;
                mediaPacket->timestamp = ntohl(rtpHeader->timestamp) + (qint64(timestampHigh) << 32);
                int ssrcTmp = (ssrc - BASIC_FFMPEG_SSRC) / 2;
                mediaPacket->channelNumber = ssrcTmp / MAX_CONTEXTS_AT_VIDEO;
                mediaPacket->subChannelNumber = ssrcTmp % MAX_CONTEXTS_AT_VIDEO;
                /*
                if (mediaPacket->timestamp < 0x40000000 && m_prevTimestamp[ssrc] > 0xc0000000)
                    m_timeStampCycles[ssrc]++;
                mediaPacket->timestamp += ((qint64) m_timeStampCycles[ssrc] << 32);
                */
            }
        }
        if (mediaPacket) {
            mediaPacket->data.write((const char*)payload, dataSize);
        }
        if (rtpHeader->marker)
        {
            result = nextPacket;
            m_nextDataPacket[ssrc] = QnAbstractDataPacketPtr(0); // EOF video frame reached
            if (mediaPacket && m_position != DATETIME_NOW)
                m_position = mediaPacket->timestamp;
        }
    }
    return result;
}

void QnRtspClientArchiveDelegate::onReverseMode(qint64 displayTime, bool value)
{
    int sign = value ? -1 : 1;
    m_rtspSession.sendPlay(displayTime, /*RTPSession::RTSP_NOW*/ qAbs(m_rtspSession.getScale()) * sign);
}
