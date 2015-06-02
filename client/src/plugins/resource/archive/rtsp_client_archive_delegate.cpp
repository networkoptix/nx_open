
#include "rtsp_client_archive_delegate.h"

#include <QtCore/QBuffer>
#include <QtNetwork/QNetworkCookie>

extern "C"
{
    #include <libavcodec/avcodec.h>
}

#include <api/app_server_connection.h>
#include <api/session_manager.h>
#include <api/network_proxy_factory.h>

#include <core/datapacket/media_data_packet.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>

#include <plugins/resource/server_camera/server_camera.h>
#include <plugins/resource/archive/archive_stream_reader.h>

#include <redass/redass_controller.h>

#include <utils/common/util.h>
#include <utils/common/sleep.h>
#include <utils/common/synctime.h>
#include <utils/media/ffmpeg_helper.h>
#include <utils/network/rtp_stream_parser.h>
#include <utils/network/ffmpeg_sdp.h>
#include <QtConcurrent/QtConcurrentFilter>
#include "http/custom_headers.h"

static const int MAX_RTP_BUFFER_SIZE = 65535;

static const int REOPEN_TIMEOUT = 1000;

QnRtspClientArchiveDelegate::QnRtspClientArchiveDelegate(QnArchiveStreamReader* reader):
    QnAbstractArchiveDelegate(),
    m_rtpData(0),
    m_tcpMode(true),
    m_position(DATETIME_NOW),
    m_opened(false),
    m_lastPacketFlags(-1),
    m_closing(false),
    m_singleShotMode(false),
    m_sendedCSec(0),
    m_lastSeekTime(AV_NOPTS_VALUE),
    m_lastReceivedTime(AV_NOPTS_VALUE),
    m_blockReopening(false),
    m_quality(MEDIA_Quality_High),
    m_qualityFastSwitch(true),
    m_lastMinTimeTime(0),
    m_globalMinArchiveTime(AV_NOPTS_VALUE),
    m_forcedEndTime(AV_NOPTS_VALUE),
    m_isMultiserverAllowed(true),
    m_playNowModeAllowed(true),
    m_reader(reader),
    m_frameCnt(0),
    m_lockedTime(AV_NOPTS_VALUE)
{
    m_rtpDataBuffer = new quint8[MAX_RTP_BUFFER_SIZE];
    m_flags |= Flag_SlowSource;
    m_flags |= Flag_CanProcessNegativeSpeed;
    m_flags |= Flag_CanProcessMediaStep;
    m_flags |= Flag_CanSendMotion;
    m_flags |= Flag_CanSeekImmediatly;

    if (reader)
        connect(this, SIGNAL(dataDropped(QnArchiveStreamReader*)), qnRedAssController, SLOT(onSlowStream(QnArchiveStreamReader*)));

    m_auth.username = QnAppServerConnectionFactory::url().userName();
    m_auth.password = QnAppServerConnectionFactory::url().password();
    m_auth.videowall = QnAppServerConnectionFactory::videowallGuid();

    connect(qnCameraHistoryPool, &QnCameraHistoryPool::cameraHistoryChanged, this, [this](const QnVirtualCameraResourcePtr &camera) {
        /* Ignore camera history if fixed server is set. */
        if (m_fixedServer || !m_isMultiserverAllowed)
            return;

        /* Ignore other cameras changes. */
        if (camera != m_camera)
            return;

        if (m_server != getServerOnTime(m_position))
            reopen();
    });

    connect(qnCameraHistoryPool, &QnCameraHistoryPool::cameraFootageChanged, this, [this](const QnVirtualCameraResourcePtr &camera) {
        /* Ignore camera history if fixed server is set. */
        if (m_fixedServer || !m_isMultiserverAllowed)
            return;

        /* Ignore other cameras changes. */
        if (camera != m_camera)
            return;

        checkMinTimeFromOtherServer(m_camera);
    });
}

void QnRtspClientArchiveDelegate::setCamera(const QnVirtualCameraResourcePtr &camera)
{
    if (m_camera == camera)
        return;

    m_camera = camera;
    m_server = camera->getParentServer();
    setupRtspSession(camera, m_server, &m_rtspSession, m_playNowModeAllowed);
}

void QnRtspClientArchiveDelegate::setFixedServer(const QnMediaServerResourcePtr &server){
    m_server = server;
    m_fixedServer = m_server;
}

QnRtspClientArchiveDelegate::~QnRtspClientArchiveDelegate() {
    close();
    delete [] m_rtpDataBuffer;
}

QnMediaServerResourcePtr QnRtspClientArchiveDelegate::getNextMediaServerFromTime(const QnVirtualCameraResourcePtr &camera, qint64 time) {
    if (!camera)
        return QnMediaServerResourcePtr();
    if (m_fixedServer)
        return QnMediaServerResourcePtr();
    return qnCameraHistoryPool->getNextMediaServerAndPeriodOnTime(camera, time, m_rtspSession.getScale() >= 0, &m_serverTimePeriod);
}

QString QnRtspClientArchiveDelegate::getUrl(const QnVirtualCameraResourcePtr &camera, const QnMediaServerResourcePtr &_server) const {
    // if camera is null we can't get its parent in any case
    QnMediaServerResourcePtr server = (_server || (!camera))
        ? _server 
        : camera->getParentServer();
    if (!server)
        return QString();
    QString url = server->getUrl() + QLatin1Char('/');
    if (camera)
        url += camera->getPhysicalId();
    else
        url += server->getUrl();
    return url;
}

struct ArchiveTimeCheckInfo
{
    ArchiveTimeCheckInfo(): owner(0), result(0) {}
    ArchiveTimeCheckInfo(const QnVirtualCameraResourcePtr& camera, const QnMediaServerResourcePtr& server, QnRtspClientArchiveDelegate* owner, qint64* result): 
        camera(camera), server(server), owner(owner), result(result) {}
    QnVirtualCameraResourcePtr camera;
    QnMediaServerResourcePtr server;
    QnRtspClientArchiveDelegate* owner;
    qint64* result;
};

void QnRtspClientArchiveDelegate::checkMinTimeFromOtherServer(const QnVirtualCameraResourcePtr &camera, const QnMediaServerResourcePtr &server, qint64* result)
{
    RTPSession otherRtspSession;
    QnRtspClientArchiveDelegate::setupRtspSession(camera, server,  &otherRtspSession, false);
    if (otherRtspSession.open(QnRtspClientArchiveDelegate::getUrl(camera, server)).errorCode != CameraDiagnostics::ErrorCode::noError) 
        return;

    qint64 startTime = otherRtspSession.startTime();
    if ((quint64)startTime != AV_NOPTS_VALUE && startTime != DATETIME_NOW) 
    {
        SCOPED_MUTEX_LOCK( lock, &m_timeMutex);
        if (startTime < *result || *result == AV_NOPTS_VALUE)
            *result = startTime;
    }
}

bool checkGlobalMinTime(const ArchiveTimeCheckInfo& checkInfo)
{
    checkInfo.owner->checkMinTimeFromOtherServer(checkInfo.camera, checkInfo.server, checkInfo.result);
    return true;
}

void QnRtspClientArchiveDelegate::checkMinTimeFromOtherServer(const QnVirtualCameraResourcePtr &camera)
{
    if (!camera) {
        SCOPED_MUTEX_LOCK( lock, &m_timeMutex);
        m_globalMinArchiveTime = AV_NOPTS_VALUE;
        return;
    }

    QnMediaServerResourceList mediaServerList = qnCameraHistoryPool->getCameraFootageData(camera);
    /* Check if no archive available on any server. */
    if (mediaServerList.isEmpty()) {
        SCOPED_MUTEX_LOCK( lock, &m_timeMutex);
        m_globalMinArchiveTime = AV_NOPTS_VALUE;
        return;
    }

    QList<ArchiveTimeCheckInfo> checkList;
    qint64 currentMinTime  = AV_NOPTS_VALUE;
    qint64 otherMinTime  = AV_NOPTS_VALUE;
    for (const auto &server: mediaServerList)
        checkList << ArchiveTimeCheckInfo(camera, server, this, server == m_server ? &currentMinTime : &otherMinTime);
    QtConcurrent::blockingFilter(checkList, checkGlobalMinTime);
    SCOPED_MUTEX_LOCK( lock, &m_timeMutex);
    if ((otherMinTime != AV_NOPTS_VALUE) && (currentMinTime == AV_NOPTS_VALUE || otherMinTime < currentMinTime))
        m_globalMinArchiveTime = otherMinTime;
    else
        m_globalMinArchiveTime = AV_NOPTS_VALUE;
}

QnMediaServerResourcePtr QnRtspClientArchiveDelegate::getServerOnTime(qint64 timeUsec) {
    if (!m_camera)
        return QnMediaServerResourcePtr();
    QnMediaServerResourcePtr currentServer = m_camera->getParentServer();

    if (timeUsec == DATETIME_NOW)
        return currentServer;

    qint64 timeMs = timeUsec / 1000;

    QnMediaServerResourcePtr mediaServer = qnCameraHistoryPool->getMediaServerOnTime(m_camera, timeMs, &m_serverTimePeriod);
    if (!mediaServer)
        return currentServer;
    
    if (mediaServer != m_server)
        qDebug() << "switch to server " << mediaServer->getUrl();
    return mediaServer;

}

bool QnRtspClientArchiveDelegate::open(const QnResourcePtr &resource) {
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    Q_ASSERT(camera);
    if (!camera)
        return false;

    setCamera(camera);

    bool rez = openInternal();
    if (!rez) {
        for (int i = 0; i < REOPEN_TIMEOUT/10 && !m_closing; ++i)
            QnSleep::msleep(10);
    }
    return rez;
}

bool QnRtspClientArchiveDelegate::openInternal() {
    if (m_opened)
        return true;
    m_closing = false;

    m_customVideoLayout.reset();
   
    if (!m_fixedServer) {
        m_server = getServerOnTime(m_position); // try to update server
        if (m_server == 0 || m_server->getStatus() == Qn::Offline) 
        {
            if (m_isMultiserverAllowed && m_globalMinArchiveTime == AV_NOPTS_VALUE)
                checkMinTimeFromOtherServer(m_camera);
            return false;
        }
    }

    setupRtspSession(m_camera, m_server, &m_rtspSession, m_playNowModeAllowed);
    m_rtpData = 0;

    const bool isOpened = m_rtspSession.open(getUrl(m_camera, m_server), m_lastSeekTime).errorCode == CameraDiagnostics::ErrorCode::noError;
    if (isOpened)
    {
        lockTime(startTime());
        if (m_isMultiserverAllowed)
            checkMinTimeFromOtherServer(m_camera);

        qint64 endTime = m_position;
        if (m_forcedEndTime)
            endTime = m_forcedEndTime;

        m_rtspSession.play(m_position, endTime, m_rtspSession.getScale());
        RTPSession::TrackMap trackInfo =  m_rtspSession.getTrackInfo();
        if (!trackInfo.isEmpty())
            m_rtpData = trackInfo[0]->ioDevice;
        if (!m_rtpData)
            m_rtspSession.stop();
        unlockTime();
    }
    else {
        m_rtspSession.stop();
    }
    m_opened = m_rtspSession.isOpened();
    m_sendedCSec = m_rtspSession.lastSendedCSeq();

    if (m_opened) {

        QList<QByteArray> audioSDP = m_rtspSession.getSdpByType(RTPSession::TT_AUDIO);
        parseAudioSDP(audioSDP);

        QString vLayout = m_rtspSession.getVideoLayout();
        if (!vLayout.isEmpty()) {
            auto newValue = QnCustomResourceVideoLayout::fromString(vLayout);
            bool isChanged =  getVideoLayout()->toString() != newValue->toString();
            m_customVideoLayout = newValue;
            if(isChanged)
                emit m_reader->videoLayoutChanged();
        }
    }
    return m_opened;
}

void QnRtspClientArchiveDelegate::parseAudioSDP(const QList<QByteArray>& audioSDP)
{
    for (int i = 0; i < audioSDP.size(); ++i)
    {
        if (audioSDP[i].startsWith("a=fmtp")) {
            int configPos = audioSDP[i].indexOf("config=");
            if (configPos > 0) {
                m_audioLayout.reset( new QnResourceCustomAudioLayout() );
                QByteArray contextData = QByteArray::fromBase64(audioSDP[i].mid(configPos + 7));
                QnMediaContextPtr context(new QnMediaContext(contextData));
                if (context->ctx() && context->ctx()->codec_type == AVMEDIA_TYPE_AUDIO)
                    m_audioLayout->addAudioTrack(QnResourceAudioLayout::AudioTrack(context, getAudioCodecDescription(context->ctx())));
            }
        }
    }
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
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    //m_waitBOF = false;
    m_rtspSession.stop();
    m_lastPacketFlags = -1;
    m_opened = false;
    m_audioLayout.reset();
    m_frameCnt = 0;
    m_parsers.clear();
}

void QnRtspClientArchiveDelegate::lockTime(qint64 value)
{
    SCOPED_MUTEX_LOCK(lock, &m_timeMutex);
    m_lockedTime = value;
}

void QnRtspClientArchiveDelegate::unlockTime()
{
    SCOPED_MUTEX_LOCK(lock, &m_timeMutex);
    m_lockedTime = AV_NOPTS_VALUE;
}

qint64 QnRtspClientArchiveDelegate::startTime()
{
    SCOPED_MUTEX_LOCK( lock, &m_timeMutex);
    if(m_lockedTime != AV_NOPTS_VALUE)
        return m_lockedTime;
    qint64 result = m_globalMinArchiveTime != AV_NOPTS_VALUE ? m_globalMinArchiveTime : m_rtspSession.startTime();

    if (result == DATETIME_NOW || result <= qnSyncTime->currentMSecsSinceEpoch()*1000)
        return result;
    else
        return DATETIME_NOW; // archive in a future
}

qint64 QnRtspClientArchiveDelegate::endTime()
{
    /*
    if (m_rtspSession.endTime() > qnSyncTime->currentUSecsSinceEpoch())
        return m_rtspSession.endTime();
    else
    */
    return DATETIME_NOW; // LIVE or archive future point as right edge for server video
}

void QnRtspClientArchiveDelegate::reopen()
{
    close();

    if (m_blockReopening)
        return;

    for (int i = 0; i < REOPEN_TIMEOUT/10 && !m_closing; ++i)
        QnSleep::msleep(10);

    if (m_camera)
        openInternal();
}

QnAbstractMediaDataPtr QnRtspClientArchiveDelegate::getNextData()
{
    QnAbstractMediaDataPtr result = getNextDataInternal();
    if (!result && !m_blockReopening)
        result = getNextDataInternal(); // try again in case of RTSP reconnect

    if (m_serverTimePeriod.isNull() || !m_isMultiserverAllowed)
        return result;
    
    // Check if archive moved to other video server
    qint64 timeMs = AV_NOPTS_VALUE;
    if (result && result->timestamp >= 0)
        timeMs = result->timestamp/1000; // do not switch server if AV_NOPTS_VALUE and any other invalid packet timings
    bool outOfRange = (quint64)timeMs != AV_NOPTS_VALUE && ((m_rtspSession.getScale() >= 0 && timeMs >= m_serverTimePeriod.endTimeMs()) ||
                      (m_rtspSession.getScale() <  0 && timeMs < m_serverTimePeriod.startTimeMs));
    if (result == 0 || outOfRange || result->dataType == QnAbstractMediaData::EMPTY_DATA)
    {
        if ((quint64)m_lastSeekTime == AV_NOPTS_VALUE)
            m_lastSeekTime = qnSyncTime->currentMSecsSinceEpoch()*1000;
        QnMediaServerResourcePtr newServer = getNextMediaServerFromTime(m_camera, m_lastSeekTime/1000);
        if (newServer) {
            qDebug() << "Reached the edge for archive in a current server. packetTime=" << QDateTime::fromMSecsSinceEpoch(timeMs).toString() <<
                "period: " << QDateTime::fromMSecsSinceEpoch(m_serverTimePeriod.startTimeMs).toString() << "-" << 
                QDateTime::fromMSecsSinceEpoch(m_serverTimePeriod.endTimeMs()).toString();

            m_server = newServer;
            m_lastSeekTime = m_serverTimePeriod.startTimeMs*1000;
            if (m_rtspSession.getScale() > 0)
                m_position = m_serverTimePeriod.startTimeMs*1000;
            else
                m_position = (m_serverTimePeriod.endTimeMs()-1)*1000;
            close();
            openInternal();

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
    QTime receiveTimer;
    receiveTimer.restart();
    while(!result)
    {
        if (!m_rtpData || (!m_opened && !m_closing)) {
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
            static const int MAX_ERROR_COUNT = 10;

            errCnt++;
            if (errCnt >= MAX_ERROR_COUNT)
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
//                qWarning() << Q_FUNC_INFO << __LINE__ << "strange RTP/TCP packet. len < 4. Ignored";
                return result;
            }
            rtpChannelNum = m_rtpDataBuffer[1];
            blockSize -= 4;
            data += 4;
        }
        else {
            rtpChannelNum = m_rtpData->getMediaSocket()->getLocalAddress().port;
        }
        const QString format = m_rtspSession.getTrackFormatByRtpChannelNum(rtpChannelNum).toLower();
        qint64 parserPosition = AV_NOPTS_VALUE;
        if (format.isEmpty()) {
            //qWarning() << Q_FUNC_INFO << __LINE__ << "RTP track" << rtpChannelNum << "not found";
        }
        else if (format == QLatin1String("ffmpeg")) {
            result = std::dynamic_pointer_cast<QnAbstractMediaData>(processFFmpegRtpPayload(data, blockSize, rtpChannelNum/2, &parserPosition));
            if (!result && m_frameCnt == 0 && receiveTimer.elapsed() > 4000)
                emit dataDropped(m_reader); // if client can't receive first frame too long inform that stream is slow
        }
        else if (format == QLatin1String("ffmpeg-metadata")) {
            processMetadata(data, blockSize);
        }
        else
            qWarning() << Q_FUNC_INFO << __LINE__ << "Only FFMPEG payload format now implemeted. Ask developers to add '" << format << "' format";

        if (result && m_sendedCSec != result->opaque)
            result.reset(); // ignore old archive data
        if (result && parserPosition != AV_NOPTS_VALUE)
            m_position = parserPosition;
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
    if (!result && !m_closing)
        reopen();
    if (result) {
        m_lastPacketFlags = result->flags;
        m_frameCnt++;
    }


    /*
    if (result && result->flags & QnAbstractMediaData::MediaFlags_LIVE)
    {
        // Server can change quality for LIVE stream (for archive quality controlled by client only)
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
    */

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
    if (m_isMultiserverAllowed) {
        QnMediaServerResourcePtr newServer = getServerOnTime(m_position);
        if (newServer != m_server)
        {
            close();
            m_server = newServer;
        }
    }

    if (!findIFrame)
        m_rtspSession.setAdditionAttribute("x-no-find-iframe", "1");

    if (!m_opened && m_camera) {
        if (!openInternal() && m_isMultiserverAllowed)
        {
            // Try next server in the list immediately, It is improve seek time if current server is offline and next server is exists
            while (!m_closing)
            {
                QnMediaServerResourcePtr nextServer = getNextMediaServerFromTime(m_camera, m_lastSeekTime/1000);
                if (nextServer && nextServer != m_server) 
                {
                    m_server = nextServer;
                    m_lastSeekTime = m_serverTimePeriod.startTimeMs*1000;
                    if (m_rtspSession.getScale() > 0)
                        m_position = m_serverTimePeriod.startTimeMs*1000;
                    else
                        m_position = (m_serverTimePeriod.endTimeMs()-1)*1000;
                    close();
                    if (openInternal())
                        break;
                }
                else {
                    break;
                }
            }
        }
    }
    else {
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

QnConstResourceVideoLayoutPtr QnRtspClientArchiveDelegate::getVideoLayout()
{
    if (m_customVideoLayout)
        return m_customVideoLayout;
    else if (m_camera)
        return m_camera->getVideoLayout();
    else
        return QnMediaResource::getDefaultVideoLayout();
}

QnConstResourceAudioLayoutPtr QnRtspClientArchiveDelegate::getAudioLayout()
{
    if (!m_audioLayout) {
        m_audioLayout.reset( new QnResourceCustomAudioLayout() );
        for (QMap<int, QnFfmpegRtpParserPtr>::const_iterator itr = m_parsers.begin(); itr != m_parsers.end(); ++itr)
        {
            QnMediaContextPtr context = itr.value()->mediaContext();
            if (context && context->ctx() && context->ctx()->codec_type == AVMEDIA_TYPE_AUDIO)
                m_audioLayout->addAudioTrack(QnResourceAudioLayout::AudioTrack(context, getAudioCodecDescription(context->ctx())));
        }
    }
    return m_audioLayout;
}

void QnRtspClientArchiveDelegate::processMetadata(const quint8* data, int dataSize)
{
   // RtpHeader* rtpHeader = (RtpHeader*) data;
    const quint8* payload = data + RtpHeader::RTP_HEADER_SIZE;
    QByteArray ba((const char*)payload, data + dataSize - payload);
    if (ba.startsWith("npt="))
        m_rtspSession.parseRangeHeader(QLatin1String(ba));
    else if (ba.startsWith("drop-report"))
        emit dataDropped(m_reader);
}

QnAbstractDataPacketPtr QnRtspClientArchiveDelegate::processFFmpegRtpPayload(quint8* data, int dataSize, int channelNum, qint64* parserPosition)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);

    QnAbstractMediaDataPtr result;

    QMap<int, QnFfmpegRtpParserPtr>::iterator itr = m_parsers.find(channelNum);
    if (itr == m_parsers.end())
        itr = m_parsers.insert(channelNum, QnFfmpegRtpParserPtr(new QnFfmpegRtpParser()));
    QnFfmpegRtpParserPtr parser = itr.value();
    bool gotData = false;
    parser->processData(data, 0, dataSize, RtspStatistic(), gotData);
    *parserPosition = parser->position();
    if (gotData) {
        result = parser->nextData();
        if (result)
            result->channelNumber = channelNum;
    }
    return result;
}

void QnRtspClientArchiveDelegate::onReverseMode(qint64 displayTime, bool value)
{
    m_blockReopening = false;
    int sign = value ? -1 : 1;
    bool fromLive = value && m_position == DATETIME_NOW;
    close();

    if (!m_opened && m_camera) {
        m_rtspSession.setScale(qAbs(m_rtspSession.getScale()) * sign);
        m_position = displayTime;
        openInternal();
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

    QByteArray value; // = quality == MEDIA_Quality_High ? "high" : "low";
    if (quality == MEDIA_Quality_ForceHigh)
        value = "force-high";
    else if (quality == MEDIA_Quality_High)
        value = "high";
    else
        value = "low";

    QByteArray paramName = "x-media-quality";
    m_rtspSession.setAdditionAttribute(paramName, value);

    if (!m_rtspSession.isOpened())
        return false;

    if (!fastSwitch) {
        m_rtspSession.sendSetParameter(paramName, value);
        return false; // switch quality without seek (it is take more time)
    }
    else 
        return true; // need send seek command
}

void QnRtspClientArchiveDelegate::setSendMotion(bool value)
{
    m_rtspSession.setAdditionAttribute("x-send-motion", value ? "1" : "0");
    m_rtspSession.sendSetParameter("x-send-motion", value ? "1" : "0");
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
    if (m_camera && m_camera->hasParam(lit("groupplay")))
        return; // avoid close/open for VMAX

    qint64 diff = qAbs(m_lastReceivedTime - qnSyncTime->currentMSecsSinceEpoch());
    bool longNoData = ((m_position == DATETIME_NOW || time == DATETIME_NOW) && diff > 250) || diff > 1000*10;
    if (longNoData || m_quality == MEDIA_Quality_Low)
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

void QnRtspClientArchiveDelegate::setupRtspSession(const QnVirtualCameraResourcePtr &camera, const QnMediaServerResourcePtr &server, RTPSession* session, bool usePredefinedTracks) const {
    if (usePredefinedTracks) {
        int numOfVideoChannels = 1;
        QnConstResourceVideoLayoutPtr videoLayout = camera->getVideoLayout(0);
         if (videoLayout)
             numOfVideoChannels = videoLayout->channelCount();
        session->setUsePredefinedTracks(numOfVideoChannels); // omit DESCRIBE and SETUP requests
    } else {
        session->setUsePredefinedTracks(0);
    }
    
    QString user = m_auth.username;
    QString password = m_auth.password;
    QAuthenticator auth;
    auth.setUser(user);
    auth.setPassword(password);
    session->setAuth(auth, RTPSession::authDigest);

    if (!m_auth.videowall.isNull())
        session->setAdditionAttribute(Qn::VIDEOWALL_GUID_HEADER_NAME, m_auth.videowall.toString().toUtf8());

    if (server) {
        QNetworkProxy proxy = QnNetworkProxyFactory::instance()->proxyToResource(server);
        if (proxy.type() != QNetworkProxy::NoProxy)
            session->setProxyAddr(proxy.hostName(), proxy.port());
        session->setAdditionAttribute(Qn::SERVER_GUID_HEADER_NAME, server->getId().toByteArray());
    }

    session->setTransport(QLatin1String("TCP"));
}

void QnRtspClientArchiveDelegate::setPlayNowModeAllowed(bool value)
{
    m_playNowModeAllowed = value;
    if (!value)
        m_rtspSession.setUsePredefinedTracks(0);
}
