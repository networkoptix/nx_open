#include "rtsp_client_dataprovider.h"
#include "resource/url_resource.h"
#include "resourcecontrol/resource_pool.h"
#include "network/rtp_stream_parser.h"
#include "ffmpeg/ffmpeg_helper.h"
#include "datapacket/mediadatapacket.h"
#include "common/base.h"

#include <qdebug.h>
#include <QFile>

static const int MAX_RTP_BUFFER_SIZE = 65535;

// current implementation for TCP mode only

//#define DEBUG_RTSP

QnRtspClientDataProvider::QnRtspClientDataProvider(QnResourcePtr res):
    QnNavigatedDataProvider(),
    QnServerPushDataProvider(res),
    m_tcpMode(true),
    m_rtpData(0),
    m_position(0)
{
    m_rtpDataBuffer = new quint8[MAX_RTP_BUFFER_SIZE];
    //m_isOpened = false;
}

QnRtspClientDataProvider::~QnRtspClientDataProvider()
{
    delete [] m_rtpDataBuffer;
    for (QMap<int, AVCodecContext*>::iterator itr = m_contextMap.begin(); itr != m_contextMap.end(); ++itr)
    {
        delete itr.value();
    }
}

QnAbstractDataPacketPtr QnRtspClientDataProvider::processFFmpegRtpPayload(const quint8* data, int dataSize)
{
    QnAbstractDataPacketPtr result(0);
    if (dataSize < RtpHeader::RTP_HEADER_SIZE)
    {
        qWarning() << Q_FUNC_INFO << __LINE__ << "strange RTP packet. len < RTP header size. Ignored";
        return result;
    }

    RtpHeader* rtpHeader = (RtpHeader*) data;
    const quint8* payload = data + RtpHeader::RTP_HEADER_SIZE;
    dataSize -= RtpHeader::RTP_HEADER_SIZE;
    quint32 ssrc = ntohl(rtpHeader->ssrc);
    bool isCodecContext = ssrc & 1; // odd numbers - codec context, even numbers - data
    if (isCodecContext)
    {
        if (!m_contextMap.contains(ssrc))
        {
            AVCodecContext* context = QnFfmpegHelper::deserializeCodecContext((const char*) payload, dataSize);
            if (context)
                m_contextMap.insert(ssrc, context);
        }
    }
    else {
        QMap<int, AVCodecContext*>::iterator itr  = m_contextMap.find(ssrc + 1);
        if (itr == m_contextMap.end())
            return result; // not codec context found
        AVCodecContext* context = itr.value();
        if (rtpHeader->padding)
            dataSize -= ntohl(rtpHeader->padding);
        if (dataSize < 1)
            return result;
        QnAbstractMediaDataPacketPtr nextPacket = m_nextDataPacket[ssrc];
        if (nextPacket == 0)
        {
            if (context->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                quint8 flags = *payload;
                dataSize--;
                payload++;
                // todo: determine total video frame size here to prevent bytearray reallocate
                QnCompressedVideoData* video = new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, dataSize, context);
                nextPacket = QnCompressedVideoDataPtr(video);
                video->keyFrame = flags & 0x80;
                video->width = context->coded_width;
                video->height = context->coded_height;
            }
            else if (context->codec_type == AVMEDIA_TYPE_AUDIO)
                nextPacket = QnCompressedAudioDataPtr(new QnCompressedAudioData(CL_MEDIA_ALIGNMENT, dataSize, context));
            else {
                qWarning() << "Unsupported RTP media type " << context->codec_type;
                return result;
            }
            m_nextDataPacket[ssrc] = nextPacket;
            nextPacket->compressionType = context->codec_id;
            nextPacket->timestamp = ntohl(rtpHeader->timestamp);
            if (nextPacket->timestamp < 0x40000000 && m_prevTimestamp[ssrc] > 0xc0000000)
                m_timeStampCycles[ssrc]++;
            nextPacket->timestamp += ((qint64) m_timeStampCycles[ssrc] << 32);
        }
        nextPacket->data.write((const char*)payload, dataSize);
        if (rtpHeader->marker)
        {
            result = nextPacket;
            m_nextDataPacket[ssrc] = QnAbstractMediaDataPacketPtr(0); // EOF video frame reached
            m_position = nextPacket->timestamp;
        }

    }
    return result;
}

QnAbstractDataPacketPtr QnRtspClientDataProvider::getNextData()
{
    // sometime function may return zero packet if no data arrived
    QnAbstractDataPacketPtr result(0);
    while (!m_needStop && result == 0)
    {
        if (!m_rtpData) 
            return result;

        int rtpChannelNum = 0;
        int blockSize  = m_rtpData->read((char*)m_rtpDataBuffer, MAX_RTP_BUFFER_SIZE);
        if (blockSize < 0) {
            m_rtspSession.stop();
            return result; // recconect
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
        const QString& format = m_rtspSession.getTrackFormat(rtpChannelNum).toLower();
        if (format.isNull()) {
            qWarning() << Q_FUNC_INFO << __LINE__ << "RTP track" << rtpChannelNum << "not found";
        }
        else if (format == "ffmpeg") 
            result = processFFmpegRtpPayload(data, blockSize);
        else 
            qWarning() << Q_FUNC_INFO << __LINE__ << "Only FFMPEG payload format now implemeted. Ask developers to add '" << format << "' format";
    }
    return result;
}

void QnRtspClientDataProvider::updateStreamParamsBasedOnQuality()
{
}

void QnRtspClientDataProvider::channeljumpTo(quint64 mksec, int channel)
{
}

void QnRtspClientDataProvider::openStream()
{
    QnClientMediaResourcePtr media = m_resource.dynamicCast<QnClientMediaResource>();
    if (!media)
        return;
    QnURLResourcePtr server = qSharedPointerDynamicCast<QnURLResource>(qnResPool.getResourceById(media->getParentId()));
    if (server == 0)
        return;
    m_rtspSession.setTransport("TCP");

    QString url = server->getUrl() + QString('/');
    QnURLResourcePtr mediaUrl = qSharedPointerDynamicCast<QnURLResource>(qnResPool.getResourceById(m_resource->getId()));
    if (mediaUrl != 0 && !mediaUrl->getUrl().isEmpty())
        url += mediaUrl->getUrl();
    else
        url += m_resource->getId().toString();
    if (m_rtspSession.open(url)) {
        m_rtpData = m_rtspSession.play(m_position);
        if (!m_rtpData)
            m_rtspSession.stop();
    }
    else {
        m_rtspSession.stop();
    }
}

void QnRtspClientDataProvider::closeStream()
{
    m_rtspSession.stop();
    m_rtpData = 0;
}

bool QnRtspClientDataProvider::isStreamOpened() const
{
    return m_rtspSession.isOpened();
}
