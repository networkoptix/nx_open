#include "rtsp_client_archive_delegate.h"
#include "core/datapacket/mediadatapacket.h"
#include "core/resourcemanagment/resource_pool.h"
#include "utils/network/rtp_stream_parser.h"
#include "libavcodec/avcodec.h"
#include "utils/media/ffmpeg_helper.h"
#include "utils/network/ffmpeg_sdp.h"
#include "utils/common/util.h"
#include "utils/common/sleep.h"
#include "utils/common/synctime.h"
#include "core/resource/camera_history.h"
#include "core/resource/video_server.h"

static const int MAX_RTP_BUFFER_SIZE = 65535;

QString QnRtspClientArchiveDelegate::m_proxyAddr;
int QnRtspClientArchiveDelegate::m_proxyPort = 0;


QnRtspClientArchiveDelegate::QnRtspClientArchiveDelegate():
    QnAbstractArchiveDelegate(),
    m_tcpMode(true),
    m_rtpData(0),
    m_position(DATETIME_NOW),
    m_opened(false),
    //m_waitBOF(false),
    m_lastPacketFlags(-1),
    m_closing(false),
    m_singleShotMode(false),
    m_lastReceivedTime(AV_NOPTS_VALUE),
    m_blockReopening(false),
    m_quality(MEDIA_Quality_High),
    m_qualityFastSwitch(true),
    m_lastSeekTime(AV_NOPTS_VALUE),
    m_sendedCSec(0),
    m_globalMinArchiveTime(AV_NOPTS_VALUE),
    m_lastMinTimeTime(0),
    m_forcedEndTime(AV_NOPTS_VALUE),
    m_isMultiserverAllowed(true)
{
    m_rtpDataBuffer = new quint8[MAX_RTP_BUFFER_SIZE];
    m_flags |= Flag_SlowSource;
    m_flags |= Flag_CanProcessNegativeSpeed;
    m_flags |= Flag_CanProcessMediaStep;
}

void QnRtspClientArchiveDelegate::setProxyAddr(const QString& addr, int port)
{
    m_proxyAddr = addr;
    m_proxyPort = port;
}

QnRtspClientArchiveDelegate::~QnRtspClientArchiveDelegate()
{
    close();
    delete [] m_rtpDataBuffer;
}

QnResourcePtr QnRtspClientArchiveDelegate::getNextVideoServerFromTime(QnResourcePtr resource, qint64 time)
{
    QnNetworkResourcePtr netRes = qSharedPointerDynamicCast<QnNetworkResource>(resource);
    if (!netRes)
        return QnResourcePtr();
	QString physicalId = netRes->getPhysicalId();
    QnCameraHistoryPtr history = QnCameraHistoryPool::instance()->getCameraHistory(physicalId);
    if (!history)
        return QnResourcePtr();
    QnVideoServerResourcePtr videoServer = history->getNextVideoServerOnTime(time, m_rtspSession.getScale() >= 0, m_serverTimePeriod);
    if (!videoServer)
        return QnResourcePtr();
    // get camera resource from other server. Unique id is physicalId + serverID
    QnResourcePtr newResource = qnResPool->getResourceByUniqId(physicalId + videoServer->getId().toString());
    return newResource;
}

QString QnRtspClientArchiveDelegate::getUrl(QnResourcePtr resource)
{
    QnResourcePtr server = qnResPool->getResourceById(resource->getParentId());
    if (!server)
        return QString();
    QString url = server->getUrl() + QString('/');
    QnNetworkResourcePtr netResource = qSharedPointerDynamicCast<QnNetworkResource>(resource);
    if (netResource != 0)
		url += netResource->getPhysicalId();
    else
        url += server->getUrl();
    return url;
}

qint64 QnRtspClientArchiveDelegate::checkMinTimeFromOtherServer(QnResourcePtr resource)
{
    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();
    if (currentTime - m_lastMinTimeTime < 5*60*1000ll)
        return AV_NOPTS_VALUE;
    m_lastMinTimeTime = currentTime;

    QnVideoServerResourcePtr currentVideoServer = qSharedPointerDynamicCast<QnVideoServerResource> (qnResPool->getResourceById(resource->getParentId()));
    if (!currentVideoServer) 
        return 0;

    QnNetworkResourcePtr netRes = qSharedPointerDynamicCast<QnNetworkResource>(resource);
    if (!netRes)
        return 0;
	QString physicalId = netRes->getPhysicalId();
    QnCameraHistoryPtr history = QnCameraHistoryPool::instance()->getCameraHistory(physicalId);
    if (!history)
        return 0;
    QnCameraTimePeriodList videoServerList = history->getOnlineTimePeriods();
    QList<QnVideoServerResourcePtr> checkServers;
    bool firstServer = true;
    for (int i = 0; i < videoServerList.size(); ++i)
    {
        QnVideoServerResourcePtr otherVideoServer = qSharedPointerDynamicCast<QnVideoServerResource> (qnResPool->getResourceById(videoServerList[i].getServerId()));
        if (!otherVideoServer)
            continue;
        if (firstServer && otherVideoServer == currentVideoServer)
            return 0; // archive starts with current server
        firstServer = false;
        if (otherVideoServer != currentVideoServer && m_rtspSession.startTime() != AV_NOPTS_VALUE)
        {
            if (!checkServers.contains(otherVideoServer))
                checkServers << otherVideoServer;
        }
    }

    foreach(QnVideoServerResourcePtr otherVideoServer, checkServers)
    {
        if (otherVideoServer == currentVideoServer)
            return 0; // archive starts with current server

        QnResourcePtr otherCamera = qnResPool->getResourceByUniqId(physicalId + otherVideoServer->getId().toString());
        RTPSession otherRtspSession;
        otherRtspSession.setProxyAddr(m_proxyAddr, m_proxyPort);
        if (otherRtspSession.open(getUrl(otherCamera)))
        {
            if (otherRtspSession.startTime() != AV_NOPTS_VALUE && otherRtspSession.startTime() != DATETIME_NOW)
            {
                return otherRtspSession.startTime();
            }
        }
    }
    return 0;
}


QnResourcePtr QnRtspClientArchiveDelegate::getResourceOnTime(QnResourcePtr resource, qint64 time)
{
    QnNetworkResourcePtr netRes = qSharedPointerDynamicCast<QnNetworkResource>(resource);
    if (!netRes)
        return resource;
	QString physicalId = netRes->getPhysicalId();

    if (time == DATETIME_NOW)
    {
        QnNetworkResourceList cameraList = qnResPool->getAllNetResourceByPhysicalId(physicalId);
        foreach(QnNetworkResourcePtr camera, cameraList) {
            if (!camera->isDisabled())
                return camera;
        }
        return resource;
    }

    QnCameraHistoryPtr history = QnCameraHistoryPool::instance()->getCameraHistory(physicalId);
    if (!history)
        return resource;
    QnVideoServerResourcePtr videoServer = history->getVideoServerOnTime(time, m_rtspSession.getScale() >= 0, m_serverTimePeriod, false);
    if (!videoServer)
        return resource;

    // get camera resource from other server. Unique id is physicalId + serverID
    QnResourcePtr newResource = qnResPool->getResourceByUniqId(physicalId + videoServer->getId().toString());
	if (newResource && newResource != resource) {
		QnVideoServerResourcePtr videoServer = qSharedPointerDynamicCast<QnVideoServerResource> (qnResPool->getResourceById(resource->getParentId()));
		if (videoServer)
			qDebug() << "switch to media server " << videoServer->getUrl();
	}
    return newResource ? newResource : resource;
}

bool QnRtspClientArchiveDelegate::open(QnResourcePtr resource)
{
    bool rez = openInternal(resource);
    if (!rez) {
        for (int i = 0; i < 50 && !m_closing; ++i)
            QnSleep::msleep(10);
    }
    return rez;
}

bool QnRtspClientArchiveDelegate::openInternal(QnResourcePtr resource)
{
    if (m_opened)
        return true;

    if (m_isMultiserverAllowed)
        resource = getResourceOnTime(resource, m_position != DATETIME_NOW ? m_position/1000 : m_position);

    m_closing = false;
    m_resource = resource;
    QnResourcePtr server = qnResPool->getResourceById(resource->getParentId());
    if (server == 0)
        return false;

    

    m_rtspSession.setTransport("TCP");

    m_rtspSession.setProxyAddr(m_proxyAddr, m_proxyPort);
    m_rtpData = 0;
    if (m_rtspSession.open(getUrl(resource))) 
    {
        qint64 globalMinTime = checkMinTimeFromOtherServer(resource);
        if (globalMinTime !=AV_NOPTS_VALUE)
            m_globalMinArchiveTime = globalMinTime;

        m_rtspSession.play(m_position, m_position, m_rtspSession.getScale());
        m_rtpData = m_rtspSession.getTrackIoByType("video");
        if (!m_rtpData)
            m_rtspSession.stop();
    }
    else {
        m_rtspSession.stop();
    }
    m_opened = m_rtspSession.isOpened();
    m_sendedCSec = m_rtspSession.lastSendedCSeq();
    return m_opened;
}

void QnRtspClientArchiveDelegate::beforeClose()
{
    //m_waitBOF = false;
    m_closing = true;
    if (m_rtpData)
        m_rtpData->getMediaSocket()->close();
}

void QnRtspClientArchiveDelegate::close()
{
    QMutexLocker lock(&m_mutex);
    //m_waitBOF = false;
    m_rtspSession.stop();
    m_rtpData = 0;
    m_lastPacketFlags = -1;
    m_nextDataPacket.clear();
    m_contextMap.clear();
    m_opened = false;
}

qint64 QnRtspClientArchiveDelegate::startTime()
{
    //qint64 minTime = QnCameraHistoryPool::instance()->getMinTime(qSharedPointerDynamicCast<QnNetworkResource> (m_resource));
    //if (minTime != AV_NOPTS_VALUE)
    //    return minTime*1000;
    if (m_globalMinArchiveTime == AV_NOPTS_VALUE)
        return AV_NOPTS_VALUE;

    if (m_globalMinArchiveTime != 0)
        return m_globalMinArchiveTime;
    else
        return m_rtspSession.startTime();
}

qint64 QnRtspClientArchiveDelegate::endTime()
{
    return DATETIME_NOW; // always use LIVE as right edge for server video
    //return m_rtspSession.endTime();
}

void QnRtspClientArchiveDelegate::reopen()
{
    close();

    if (m_blockReopening)
        return;

    for (int i = 0; i < 50 && !m_closing; ++i)
        QnSleep::msleep(10);

    if (m_resource)
        openInternal(m_resource);
}

QnAbstractMediaDataPtr QnRtspClientArchiveDelegate::getNextData()
{
    QnAbstractMediaDataPtr result = getNextDataInternal();
    if (m_serverTimePeriod.isNull() || !m_isMultiserverAllowed)
        return result;
    
    // Check if archive moved to other video server
    qint64 timeMs = result ? result->timestamp/1000 : 0;
    bool outOfRange = m_rtspSession.getScale() >= 0 && timeMs >= m_serverTimePeriod.endTimeMs() || 
                      m_rtspSession.getScale() <  0 && timeMs < m_serverTimePeriod.startTimeMs;
    if (result == 0 || outOfRange || result->dataType == QnAbstractMediaData::EMPTY_DATA)
    {
		qDebug() << "Reached the edge for archive in a current server. packetTime=" << QDateTime::fromMSecsSinceEpoch(timeMs).toString() <<
			        "period: " << QDateTime::fromMSecsSinceEpoch(m_serverTimePeriod.startTimeMs).toString() << "-" << 
					QDateTime::fromMSecsSinceEpoch(m_serverTimePeriod.endTimeMs()).toString();

		if (m_lastSeekTime == AV_NOPTS_VALUE)
			m_lastSeekTime = qnSyncTime->currentMSecsSinceEpoch()*1000;
		QnResourcePtr newResource = getNextVideoServerFromTime(m_resource, m_lastSeekTime/1000);
		if (newResource) {
			m_lastSeekTime = m_serverTimePeriod.startTimeMs*1000;
			if (m_rtspSession.getScale() > 0)
				m_position = m_serverTimePeriod.startTimeMs*1000;
			else
				m_position = (m_serverTimePeriod.endTimeMs()-1)*1000;
			close();
			openInternal(newResource);

            result = getNextData();
            if (result)
                result->flags |= QnAbstractMediaData::MediaFlags_NewServer;
			return result;
		}
		else {
			m_serverTimePeriod.clear();
		}
    }

    return result;
}

QnAbstractMediaDataPtr QnRtspClientArchiveDelegate::getNextDataInternal()
{
    // sometime function may return zero packet if no data arrived
    QnAbstractMediaDataPtr result;
    int errCnt = 0;
    while(!result)
    {
        if (!m_rtpData){
            //m_rtspSession.stop(); // reconnect
            reopen();
            return result;
        }

        int rtpChannelNum = 0;
        int blockSize  = m_rtpData->read((char*)m_rtpDataBuffer, MAX_RTP_BUFFER_SIZE);
        if (blockSize < 0 && !m_closing) {
            //m_rtspSession.stop(); // reconnect
            reopen();
            return result; 
        }
        else if (blockSize == 0) {
            errCnt++;
            if (errCnt == 10)
                break;
            else
                continue;
        }
        errCnt = 0;

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
            rtpChannelNum = m_rtpData->getMediaSocket()->getLocalPort();
        }
        const QString format = m_rtspSession.getTrackFormat(rtpChannelNum).toLower();
        if (format.isEmpty()) {
            //qWarning() << Q_FUNC_INFO << __LINE__ << "RTP track" << rtpChannelNum << "not found";
        }
        else if (format == QLatin1String("ffmpeg")) {
            result = qSharedPointerDynamicCast<QnAbstractMediaData>(processFFmpegRtpPayload(data, blockSize));
        }
        else if (format == QLatin1String("ffmpeg-metadata")) {
            processMetadata(data, blockSize);
        }
        else
            qWarning() << Q_FUNC_INFO << __LINE__ << "Only FFMPEG payload format now implemeted. Ask developers to add '" << format << "' format";

        if (result && m_sendedCSec != result->opaque)
            result.clear(); // ignore old archive data
        /*
        if (result && m_waitBOF)
        {
            if ((result->flags & QnAbstractMediaData::MediaFlags_BOF) &&
                ((result->flags & QnAbstractMediaData::MediaFlags_LIVE) ||  m_sendedCSec == result->opaque))
            {
                if (qAbs(result->timestamp - m_lastSeekTime) > 1000ll*1000*10)
                {
                    QString s;
                    QTextStream str(&s);
                    str << "recvSsrc=" << m_sendedCSec 
                        << " expectedTime=" << QDateTime::fromMSecsSinceEpoch(m_lastSeekTime/1000).toString("hh:mm:ss.zzz")
                        << " packetTime=" << QDateTime::fromMSecsSinceEpoch(result->timestamp/1000).toString("hh:mm:ss.zzz");
                    str.flush();
                    cl_log.log(s, cl_logALWAYS);
                }
                m_waitBOF = false;
            }
            else
                result.clear();
        }
        */
    }
    if (!result)
        reopen();
    if (result)
        m_lastPacketFlags = result->flags;


    if (result && result->flags & QnAbstractMediaData::MediaFlags_LIVE)
    {
        // Media server can change quality for LIVE stream (for archive quality controlled by client only)
        // So, if server is changed quality, update current quality variables
        if (qSharedPointerDynamicCast<QnCompressedVideoData>(result))
        {
            bool isLowPacket = result->flags & QnAbstractMediaData::MediaFlags_LowQuality;
            bool isLowQuality = m_quality == MEDIA_Quality_Low;
            if (isLowPacket != isLowQuality) 
            {
                m_rtspSession.setAdditionAttribute("x-media-quality", isLowPacket ? "low" : "high");
                m_qualityFastSwitch = true; // We already have got new quality. So, it is "fast" switch
                m_quality = isLowPacket ? MEDIA_Quality_Low : MEDIA_Quality_High;
                emit qualityChanged(m_quality);
            }
        }
    }

    m_lastReceivedTime = qnSyncTime->currentMSecsSinceEpoch();
    return result;
}

void QnRtspClientArchiveDelegate::setAdditionalAttribute(const QByteArray& name, const QByteArray& value)
{
    m_rtspSession.setAdditionAttribute(name, value);
}

qint64 QnRtspClientArchiveDelegate::seek(qint64 startTime, qint64 endTime)
{
    m_forcedEndTime = endTime;
    return seek(startTime, true);
}


qint64 QnRtspClientArchiveDelegate::seek(qint64 time, bool findIFrame)
{
    m_blockReopening = false;

    //if (time == m_position)
    //    return time;

    //deleteContexts(); // context is going to create again on first data after SEEK, so ignore rest of data before seek
    m_lastSeekTime = m_position = time;
    QnResourcePtr newResource;
    if (m_isMultiserverAllowed) {
        newResource = getResourceOnTime(m_resource, m_position/1000);
        if (newResource)
        {
            if (newResource != m_resource)
                close();
            m_resource = newResource;
        }
    }

    if (!m_opened && m_resource) {
        if (!openInternal(m_resource) && m_isMultiserverAllowed)
        {
            // Try next server in the list immediatly, It is improve seek time if current server is offline and next server is exists
            while (!m_closing)
            {
                QnResourcePtr nextResource = getNextVideoServerFromTime(m_resource, m_lastSeekTime/1000);
                if (nextResource && nextResource != m_resource) 
                {
                    m_resource = nextResource;
                    m_lastSeekTime = m_serverTimePeriod.startTimeMs*1000;
                    if (m_rtspSession.getScale() > 0)
                        m_position = m_serverTimePeriod.startTimeMs*1000;
                    else
                        m_position = (m_serverTimePeriod.endTimeMs()-1)*1000;
                    close();
                    if (openInternal(newResource))
                        break;
                }
                else {
                    break;
                }
            }
        }
    }
    else {
        if (!findIFrame)
            m_rtspSession.setAdditionAttribute("x-no-find-iframe", "1");
        qint64 endTime = AV_NOPTS_VALUE;
        if (m_forcedEndTime)
            endTime = m_forcedEndTime;
        else if (m_singleShotMode)
            endTime = time;
        m_rtspSession.sendPlay(time, endTime, m_rtspSession.getScale());
        m_rtspSession.removeAdditionAttribute("x-no-find-iframe");
    }
    m_sendedCSec = m_rtspSession.lastSendedCSeq();
    return time;
}

void QnRtspClientArchiveDelegate::setSingleshotMode(bool value)
{
    if (value == m_singleShotMode)
        return;

    m_singleShotMode = value;
    /*
    if (value)
        m_rtspSession.sendPause();
    else
        m_rtspSession.sendPlay(AV_NOPTS_VALUE, AV_NOPTS_VALUE, m_rtspSession.getScale());
    */
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

void QnRtspClientArchiveDelegate::processMetadata(const quint8* data, int dataSize)
{
    RtpHeader* rtpHeader = (RtpHeader*) data;
    const quint8* payload = data + RtpHeader::RTP_HEADER_SIZE;
    QByteArray ba((const char*)payload, data + dataSize - payload);
    if (ba.startsWith("npt="))
        m_rtspSession.parseRangeHeader(ba);
}

QnAbstractDataPacketPtr QnRtspClientArchiveDelegate::processFFmpegRtpPayload(const quint8* data, int dataSize)
{
    QMutexLocker lock(&m_mutex);

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
        QnMediaContextPtr context(new QnMediaContext(payload, dataSize));
        m_contextMap[ssrc] = context;
    }
    else
    {
        QMap<int, QnMediaContextPtr>::iterator itr = m_contextMap.find(ssrc + 1);
        QnMediaContextPtr context;
        if (itr != m_contextMap.end())
            context = itr.value();
        //else 
        //    return result;

        if (rtpHeader->padding)
            dataSize -= ntohl(rtpHeader->padding);
        if (dataSize < 1)
            return result;

        QnAbstractDataPacketPtr nextPacket = m_nextDataPacket.value(ssrc);
        QnAbstractMediaDataPtr mediaPacket = qSharedPointerDynamicCast<QnAbstractMediaData>(nextPacket);
        if (!nextPacket)
        {
            if (dataSize < RTSP_FFMPEG_GENERIC_HEADER_SIZE)
                return result;

            QnAbstractMediaData::DataType dataType = (QnAbstractMediaData::DataType) *payload++;
            quint32 timestampHigh = ntohl(*(quint32*) payload);
            dataSize -= RTSP_FFMPEG_GENERIC_HEADER_SIZE;
            payload  += 4; // deserialize timeStamp high part

            quint8 cseq = *payload++;
            quint16 flags = (payload[0]<<8) + payload[1];
            payload += 2;

            if (dataType == QnAbstractMediaData::EMPTY_DATA)
            {
                nextPacket = QnEmptyMediaDataPtr(new QnEmptyMediaData());
				if (dataSize != 0)
				{
					qWarning() << "Unexpected data size for EOF/BOF packet. got" << dataSize << "expected" << 0 << "bytes. Packet ignored.";
					return result;
				}
            }
            else if (dataType == QnAbstractMediaData::META_V1)
            {
                if (dataSize != MD_WIDTH*MD_HEIGHT/8 + RTSP_FFMPEG_METADATA_HEADER_SIZE)
                {
                    qWarning() << "Unexpected data size for metadata. got" << dataSize << "expected" << MD_WIDTH*MD_HEIGHT/8 << "bytes. Packet ignored.";
                    return result;
                }
                if (dataSize < RTSP_FFMPEG_METADATA_HEADER_SIZE)
                    return result;

                QnMetaDataV1 *metadata = new QnMetaDataV1(); 
                metadata->data.clear();
                context.clear();
                metadata->m_duration = ntohl(*((quint32*)payload))*1000;
                dataSize -= RTSP_FFMPEG_METADATA_HEADER_SIZE;
                payload += RTSP_FFMPEG_METADATA_HEADER_SIZE; // deserialize video flags

                nextPacket = QnMetaDataV1Ptr(metadata);
            }
            else if (context && context->ctx() && context->ctx()->codec_type == AVMEDIA_TYPE_VIDEO && dataType == QnAbstractMediaData::VIDEO)
            {
                if (dataSize < RTSP_FFMPEG_VIDEO_HEADER_SIZE)
                    return result;

                quint32 fullPayloadLen = (payload[0] << 16) + (payload[1] << 8) + payload[2];

                dataSize -= RTSP_FFMPEG_VIDEO_HEADER_SIZE;
                payload += RTSP_FFMPEG_VIDEO_HEADER_SIZE; // deserialize video flags

                QnCompressedVideoData *video = new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, fullPayloadLen, context);
                nextPacket = QnCompressedVideoDataPtr(video);
                if (context) 
                {
                    video->width = context->ctx()->coded_width;
                    video->height = context->ctx()->coded_height;
                }
            }
            else if (context && context->ctx() && context->ctx()->codec_type == AVMEDIA_TYPE_AUDIO && dataType == QnAbstractMediaData::AUDIO)
            {
                QnCompressedAudioData *audio = new QnCompressedAudioData(CL_MEDIA_ALIGNMENT, dataSize); // , context
                audio->context = context;
                //audio->format.fromAvStream(context->ctx());
                nextPacket = QnCompressedAudioDataPtr(audio);
            }
            else
            {
                if (context && context->ctx())
                    qWarning() << "Unsupported RTP codec or packet type. codec=" << context->ctx()->codec_type;
                else if (dataType == QnAbstractMediaData::AUDIO)
                    qWarning() << "Unsupported audio codec or codec params";
                else if (dataType == QnAbstractMediaData::VIDEO)
                    qWarning() << "Unsupported video or codec params";
                else
                    qWarning() << "Unsupported or unknown packet";
                return result;
            }

            m_nextDataPacket[ssrc] = nextPacket;
            mediaPacket = qSharedPointerDynamicCast<QnAbstractMediaData>(nextPacket);
            if (mediaPacket) 
            {
                mediaPacket->opaque = cseq;
                mediaPacket->flags = flags;

                if (context)
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
            if (mediaPacket)
            {
                if (mediaPacket->flags & QnAbstractMediaData::MediaFlags_LIVE)
                    m_position = DATETIME_NOW;
                else
                    m_position = mediaPacket->timestamp;
            }
        }
    }
    return result;
}

void QnRtspClientArchiveDelegate::onReverseMode(qint64 displayTime, bool value)
{
    m_blockReopening = false;
    int sign = value ? -1 : 1;
    bool fromLive = value && m_position == DATETIME_NOW;

    if (!m_opened && m_resource) {
        m_rtspSession.setScale(qAbs(m_rtspSession.getScale()) * sign);
        m_position = displayTime;
        openInternal(m_resource);
    }
    else {
        m_rtspSession.sendPlay(displayTime, AV_NOPTS_VALUE, qAbs(m_rtspSession.getScale()) * sign);
    }
    m_sendedCSec = m_rtspSession.lastSendedCSeq();
    //m_waitBOF = true;

    if (fromLive) 
        m_position = AV_NOPTS_VALUE;
}

bool QnRtspClientArchiveDelegate::isRealTimeSource() const 
{ 
    if (m_lastPacketFlags == -1)
        return m_position == DATETIME_NOW; 
    else
        return m_lastPacketFlags & QnAbstractMediaData::MediaFlags_LIVE;
}

bool QnRtspClientArchiveDelegate::setQuality(MediaQuality quality, bool fastSwitch)
{
    if (m_quality == quality && fastSwitch <= m_qualityFastSwitch)
        return false;
    m_quality = quality;
    m_qualityFastSwitch = fastSwitch;

    QByteArray value = quality == MEDIA_Quality_High ? "high" : "low";
    QByteArray paramName = "x-media-quality";
    m_rtspSession.setAdditionAttribute(paramName, value);

    if (!m_rtspSession.isOpened())
        return false;

    // in live mode I have swiching quality without seek for improving smooth quality
    if (isRealTimeSource() || !fastSwitch) {
        m_rtspSession.sendSetParameter(paramName, value);
        return false;
    }
    else 
        return true;
}

void QnRtspClientArchiveDelegate::setSendMotion(bool value)
{
    m_rtspSession.setAdditionAttribute("x-send-motion", value ? "1" : "0");
    //m_rtspSession.sendPlay(AV_NOPTS_VALUE, AV_NOPTS_VALUE, m_rtspSession.getScale());
}

void QnRtspClientArchiveDelegate::setMotionRegion(const QRegion& region)
{
    if (region.isEmpty())
    {
        m_rtspSession.removeAdditionAttribute("x-motion-region");
    }
    else 
    {
        QBuffer buffer;
        buffer.open(QIODevice::WriteOnly);
        QDataStream stream(&buffer);
        stream << region;
        buffer.close();
        QByteArray s = buffer.data().toBase64();
        m_rtspSession.setAdditionAttribute("x-motion-region", s);
    }
    //m_rtspSession.sendPlay(AV_NOPTS_VALUE, AV_NOPTS_VALUE, m_rtspSession.getScale());
}

void QnRtspClientArchiveDelegate::beforeSeek(qint64 time)
{
    qint64 diff = qAbs(m_lastReceivedTime - qnSyncTime->currentMSecsSinceEpoch());
    if ((m_position == DATETIME_NOW || time == DATETIME_NOW) && diff > 250 || diff > 1000*10)
    {
        m_blockReopening = true;
        close();
    }
}

void QnRtspClientArchiveDelegate::beforeChangeReverseMode(bool reverseMode)
{
    // Reconnect If camera is offline and it is switch from live to archive
    if (m_position == DATETIME_NOW) {
        if (reverseMode)
            beforeSeek(AV_NOPTS_VALUE);
        else
            m_blockReopening = false;
    }
}

void QnRtspClientArchiveDelegate::setRange(qint64 startTime, qint64 endTime, qint64 frameStep)
{
    setAdditionalAttribute("x-media-step", QByteArray::number(frameStep));
    seek(startTime, endTime);
}

void QnRtspClientArchiveDelegate::setMultiserverAllowed(bool value)
{
    m_isMultiserverAllowed = value;
}
