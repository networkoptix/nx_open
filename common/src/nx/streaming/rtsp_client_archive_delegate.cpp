
#include "rtsp_client_archive_delegate.h"

#include <QtCore/QBuffer>
#include <QtCore/QUrl>

#include <QtNetwork/QNetworkCookie>

extern "C"
{
// For declarations only.
#include <libavcodec/avcodec.h>
}

#include <api/app_server_connection.h>
#include <api/session_manager.h>
#include <api/network_proxy_factory.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_media_layout.h>
#include <utils/common/util.h>
#include <utils/common/sleep.h>
#include <utils/common/synctime.h>
#include <utils/media/av_codec_helper.h>
#include <nx/streaming/rtp_stream_parser.h>
#include <network/ffmpeg_sdp.h>
#include <QtConcurrent/QtConcurrentFilter>
#include <nx/network/http/custom_headers.h>

#include <nx/streaming/archive_stream_reader.h>
#include <nx/streaming/rtsp_client.h>
#include <nx/streaming/nx_rtp_parser.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/streaming/basic_media_context.h>
#include <nx/fusion/serialization/lexical_enum.h>
#include <api/global_settings.h>

static const int MAX_RTP_BUFFER_SIZE = 65536;
static const int REOPEN_TIMEOUT = 1000;

namespace
{
    static const QByteArray kMediaQualityParamName = "x-media-quality";
    static const QByteArray kResolutionParamName = "x-resolution";

    QString resolutionToString(const QSize& size)
    {
        if (size.width() > 0)
            return lit("%1x%2").arg(size.width()).arg(size.height());
        else
            return lit("%1p").arg(size.height());
    }
}

QnRtspClientArchiveDelegate::QnRtspClientArchiveDelegate(QnArchiveStreamReader* reader)
    :
    QnAbstractArchiveDelegate(),
	m_rtspSession(new QnRtspClient(/*shouldGuessAuthDigest*/ true)),
    m_rtpData(0),
    m_tcpMode(true),
    m_position(DATETIME_NOW),
    m_opened(false),
    m_lastMediaFlags(-1),
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
    m_maxSessionDurartionMs(std::numeric_limits<qint64>::max())
{
    m_footageUpToDate.test_and_set();
    m_currentServerUpToDate.test_and_set();
    m_rtpDataBuffer = new quint8[MAX_RTP_BUFFER_SIZE];
    m_flags |= Flag_SlowSource;
    m_flags |= Flag_CanProcessNegativeSpeed;
    m_flags |= Flag_CanProcessMediaStep;
    m_flags |= Flag_CanSendMotion;
    m_flags |= Flag_CanSeekImmediatly;
}

void QnRtspClientArchiveDelegate::setCamera(const QnSecurityCamResourcePtr &camera)
{
    if (m_camera == camera)
        return;

    if (m_camera)
        disconnect(m_camera->commonModule()->cameraHistoryPool(), nullptr, this, nullptr);

    m_camera = camera;

    NX_ASSERT(camera);
    if (!m_camera)
        return;

    m_server = camera->getParentServer();

    auto commonModule = camera->commonModule();
    auto maxSessionDurartion = commonModule->globalSettings()->maxRtspConnectDuration();
    if (maxSessionDurartion.count() > 0)
        m_maxSessionDurartionMs = maxSessionDurartion;

    m_auth.username = commonModule->currentUrl().userName();
    m_auth.password = commonModule->currentUrl().password();
    m_auth.videowall = commonModule->videowallGuid();

    connect(commonModule->cameraHistoryPool(),
        &QnCameraHistoryPool::cameraHistoryChanged,
        this,
        [this](const QnSecurityCamResourcePtr &camera)
    {
        /* Ignore camera history if fixed server is set. */
        if (m_fixedServer || !m_isMultiserverAllowed)
            return;

        /* Ignore other cameras changes. */
        if (camera != m_camera)
            return;

        if (m_server != getServerOnTime(m_position))
            m_currentServerUpToDate.clear();
    });

    connect(commonModule->cameraHistoryPool(),
        &QnCameraHistoryPool::cameraFootageChanged,
        this,
        [this](const QnSecurityCamResourcePtr &camera) {
        /* Ignore camera history if fixed server is set. */
        if (m_fixedServer || !m_isMultiserverAllowed)
            return;

        /* Ignore other cameras changes. */
        if (camera != m_camera)
            return;
        m_footageUpToDate.clear();
    });

    setupRtspSession(camera, m_server, m_rtspSession.get(), m_playNowModeAllowed);
}

void QnRtspClientArchiveDelegate::setFixedServer(const QnMediaServerResourcePtr &server){
    m_server = server;
    m_fixedServer = m_server;
}

QnRtspClientArchiveDelegate::~QnRtspClientArchiveDelegate() {
    close();
    delete [] m_rtpDataBuffer;
}

QnMediaServerResourcePtr QnRtspClientArchiveDelegate::getNextMediaServerFromTime(const QnSecurityCamResourcePtr &camera, qint64 time)
{
    if (!camera)
        return QnMediaServerResourcePtr();
    if (m_fixedServer)
        return QnMediaServerResourcePtr();
    return camera->commonModule()->cameraHistoryPool()->getNextMediaServerAndPeriodOnTime(camera, time, m_rtspSession->getScale() >= 0, &m_serverTimePeriod);
}

QString QnRtspClientArchiveDelegate::getUrl(const QnSecurityCamResourcePtr &camera, const QnMediaServerResourcePtr &_server) const {
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
    ArchiveTimeCheckInfo(const QnSecurityCamResourcePtr& camera, const QnMediaServerResourcePtr& server, QnRtspClientArchiveDelegate* owner, qint64* result):
        camera(camera), server(server), owner(owner), result(result) {}
    QnSecurityCamResourcePtr camera;
    QnMediaServerResourcePtr server;
    QnRtspClientArchiveDelegate* owner;
    qint64* result;
};

void QnRtspClientArchiveDelegate::checkGlobalTimeAsync(const QnSecurityCamResourcePtr &camera, const QnMediaServerResourcePtr &server, qint64* result)
{
    QnRtspClient otherRtspSession(true);
    QnRtspClientArchiveDelegate::setupRtspSession(camera, server,  &otherRtspSession, false);
    if (otherRtspSession.open(QnRtspClientArchiveDelegate::getUrl(camera, server)).errorCode != CameraDiagnostics::ErrorCode::noError)
        return;

    qint64 startTime = otherRtspSession.startTime();
    if (startTime != qint64(AV_NOPTS_VALUE) && startTime != DATETIME_NOW)
    {
        if (startTime < *result || *result == qint64(AV_NOPTS_VALUE))
            *result = startTime;
    }
}

void QnRtspClientArchiveDelegate::checkMinTimeFromOtherServer(const QnSecurityCamResourcePtr &camera)
{
    if (!camera || !camera->resourcePool())
    {
        m_globalMinArchiveTime = qint64(AV_NOPTS_VALUE);
        return;
    }

    QnMediaServerResourceList mediaServerList = camera->commonModule()->cameraHistoryPool()->getCameraFootageData(camera, true);

    /* Check if no archive available on any server. */
    /* Or check if archive belong to the current server only */
    if (mediaServerList.isEmpty() ||
        (mediaServerList.size() == 1 && mediaServerList[0] == camera->getParentServer()))
    {
        m_globalMinArchiveTime = qint64(AV_NOPTS_VALUE);
        return;
    }

    QList<ArchiveTimeCheckInfo> checkList;
    qint64 currentMinTime  = qint64(AV_NOPTS_VALUE);
    qint64 otherMinTime  = qint64(AV_NOPTS_VALUE);
    for (const auto &server: mediaServerList)
        checkList << ArchiveTimeCheckInfo(camera, server, this, server == m_server ? &currentMinTime : &otherMinTime);
    QtConcurrent::blockingFilter(checkList, [](const ArchiveTimeCheckInfo& checkInfo)
    {
            checkInfo.owner->checkGlobalTimeAsync(checkInfo.camera, checkInfo.server, checkInfo.result);
            return true;
    });
    if ((otherMinTime != qint64(AV_NOPTS_VALUE)) && (currentMinTime == qint64(AV_NOPTS_VALUE) || otherMinTime < currentMinTime))
        m_globalMinArchiveTime = otherMinTime;
    else
        m_globalMinArchiveTime = qint64(AV_NOPTS_VALUE);
}

QnMediaServerResourcePtr QnRtspClientArchiveDelegate::getServerOnTime(qint64 timeUsec) {
    if (!m_camera)
        return QnMediaServerResourcePtr();
    QnMediaServerResourcePtr currentServer = m_camera->getParentServer();

    if (timeUsec == DATETIME_NOW)
        return currentServer;

    qint64 timeMs = timeUsec / 1000;

    QnMediaServerResourcePtr mediaServer = m_camera->commonModule()->cameraHistoryPool()->getMediaServerOnTime(m_camera, timeMs, &m_serverTimePeriod);
    if (!mediaServer)
        return currentServer;

    if (mediaServer != m_server)
        qDebug() << "switch to server " << mediaServer->getUrl();
    return mediaServer;

}

bool QnRtspClientArchiveDelegate::open(const QnResourcePtr &resource,
    AbstractArchiveIntegrityWatcher * /*archiveIntegrityWatcher*/)
{
    QnSecurityCamResourcePtr camera = resource.dynamicCast<QnSecurityCamResource>();
    NX_ASSERT(camera);
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

bool QnRtspClientArchiveDelegate::openInternal()
{
    if (m_opened)
        return true;
    m_closing = false;
    m_sessionTimeout.restart();
    m_customVideoLayout.reset();
    m_globalMinArchiveTime = startTime(); // force current value to avoid flicker effect while current server is being changed

    if (!m_fixedServer) {
        m_server = getServerOnTime(m_position); // try to update server
        if (m_server == 0 || m_server->getStatus() == Qn::Offline)
        {
            if (m_isMultiserverAllowed && m_globalMinArchiveTime == qint64(AV_NOPTS_VALUE))
                checkMinTimeFromOtherServer(m_camera);
            return false;
        }
    }

    setupRtspSession(m_camera, m_server, m_rtspSession.get(), m_playNowModeAllowed);
    setRtpData(0);

    const bool isOpened = m_rtspSession->open(getUrl(m_camera, m_server), m_lastSeekTime).errorCode == CameraDiagnostics::ErrorCode::noError;
    if (isOpened)
    {
        qint64 endTime = m_position;
        if (m_forcedEndTime)
            endTime = m_forcedEndTime;

        m_rtspSession->play(m_position, endTime, m_rtspSession->getScale());

        if (m_isMultiserverAllowed)
            checkMinTimeFromOtherServer(m_camera);

        QnRtspClient::TrackMap trackInfo =  m_rtspSession->getTrackInfo();
        if (!trackInfo.isEmpty())
            setRtpData(trackInfo[0]->ioDevice);
        if (!m_rtpData)
            m_rtspSession->stop();
    }
    else {
        m_rtspSession->stop();
    }
    m_opened = m_rtspSession->isOpened();
    m_sendedCSec = m_rtspSession->lastSendedCSeq();

    if (m_opened) {

        QList<QByteArray> audioSDP = m_rtspSession->getSdpByType(QnRtspClient::TT_AUDIO);
        parseAudioSDP(audioSDP);

        QString vLayout = m_rtspSession->getVideoLayout();
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
                QnConstMediaContextPtr context(QnBasicMediaContext::deserialize(
                    QByteArray::fromBase64(audioSDP[i].mid(configPos + 7))));
                if (context && context->getCodecType() == AVMEDIA_TYPE_AUDIO)
                {
                    m_audioLayout->addAudioTrack(QnResourceAudioLayout::AudioTrack(
                        context, context->getAudioCodecDescription()));
                }
            }
        }
    }
}

void QnRtspClientArchiveDelegate::setRtpData(QnRtspIoDevice* value)
{
    m_rtpData = value;
}

void QnRtspClientArchiveDelegate::beforeClose()
{
    //m_waitBOF = false;
    m_closing = true;
    m_rtspSession->shutdown();
}

void QnRtspClientArchiveDelegate::close()
{
    QnMutexLocker lock( &m_mutex );
    //m_waitBOF = false;
    m_rtspSession->shutdown();
    m_lastMediaFlags = -1;
    m_opened = false;
    m_audioLayout.reset();
    m_frameCnt = 0;
    m_parsers.clear();
}

qint64 QnRtspClientArchiveDelegate::startTime() const
{
    qint64 result = m_globalMinArchiveTime;
    if (result == qint64(AV_NOPTS_VALUE))
        result = m_rtspSession->startTime();

    if (result == DATETIME_NOW || result <= qnSyncTime->currentMSecsSinceEpoch()*1000)
        return result;
    else
        return DATETIME_NOW; // archive in a future
}

qint64 QnRtspClientArchiveDelegate::endTime() const
{
    /*
    if (m_rtspSession->endTime() > qnSyncTime->currentUSecsSinceEpoch())
        return m_rtspSession->endTime();
    else
    */
    return DATETIME_NOW; // LIVE or archive future point as right edge for server video
}

void QnRtspClientArchiveDelegate::reopen()
{
    close();

    if (m_blockReopening)
        return;

    if (m_reopenTimer.isValid() && m_reopenTimer.elapsed() < REOPEN_TIMEOUT)
    {
        for (int i = 0; i < (REOPEN_TIMEOUT - m_reopenTimer.elapsed()) / 10 && !m_closing; ++i)
            QnSleep::msleep(10);
    }
    m_reopenTimer.restart();

    if (m_camera)
        openInternal();
}

QnAbstractMediaDataPtr QnRtspClientArchiveDelegate::getNextData()
{
    if (!m_currentServerUpToDate.test_and_set() ||
        (m_sessionTimeout.isValid() && m_sessionTimeout.hasExpired(m_maxSessionDurartionMs.count())))
    {
        reopen();
    }

    if (!m_footageUpToDate.test_and_set()) {
        if (m_isMultiserverAllowed)
            checkMinTimeFromOtherServer(m_camera);
    }

    QnAbstractMediaDataPtr result = getNextDataInternal();
    if (!result && !m_blockReopening && !m_closing)
        result = getNextDataInternal(); // try again in case of RTSP reconnect

    if (m_serverTimePeriod.isNull() || !m_isMultiserverAllowed)
        return result;

    // Check if archive moved to other video server
    qint64 timeMs = AV_NOPTS_VALUE;
    if (result && result->timestamp >= 0)
        timeMs = result->timestamp/1000; // do not switch server if AV_NOPTS_VALUE and any other invalid packet timings
    bool outOfRange = timeMs != AV_NOPTS_VALUE && ((m_rtspSession->getScale() >= 0 && timeMs >= m_serverTimePeriod.endTimeMs()) ||
                      (m_rtspSession->getScale() <  0 && timeMs < m_serverTimePeriod.startTimeMs));
    if (result == 0 || outOfRange || result->dataType == QnAbstractMediaData::EMPTY_DATA)
    {
        if (m_lastSeekTime == AV_NOPTS_VALUE)
            m_lastSeekTime = qnSyncTime->currentMSecsSinceEpoch()*1000;
        QnMediaServerResourcePtr newServer = getNextMediaServerFromTime(m_camera, m_lastSeekTime/1000);
        if (newServer) {
            qDebug() << "Reached the edge for archive in a current server. packetTime=" << QDateTime::fromMSecsSinceEpoch(timeMs).toString() <<
                "period: " << QDateTime::fromMSecsSinceEpoch(m_serverTimePeriod.startTimeMs).toString() << "-" <<
                QDateTime::fromMSecsSinceEpoch(m_serverTimePeriod.endTimeMs()).toString();

            m_server = newServer;
            m_lastSeekTime = m_serverTimePeriod.startTimeMs*1000;
            if (m_rtspSession->getScale() > 0)
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
    //int errCnt = 0;
    QTime receiveTimer;
    receiveTimer.restart();
    while(!result)
    {
        if (!m_rtpData || (!m_opened && !m_closing)) {
            //m_rtspSession->stop(); // reconnect
            reopen();
            return result;
        }

        int rtpChannelNum = 0;
        int blockSize  = m_rtpData->read((char*)m_rtpDataBuffer, MAX_RTP_BUFFER_SIZE);
        if (blockSize <= 0 && !m_closing) {
            //m_rtspSession->stop(); // reconnect
            reopen();
            return result;
        }
        /*
        else if (blockSize == 0) {
            static const int MAX_ERROR_COUNT = 10;

            errCnt++;
            if (errCnt >= MAX_ERROR_COUNT)
                break;
            else
                continue;
        }
        errCnt = 0;
        */

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
        const QString format = m_rtspSession->getTrackFormatByRtpChannelNum(rtpChannelNum).toLower();
        qint64 parserPosition = qint64(AV_NOPTS_VALUE);
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
        if (result && parserPosition != qint64(AV_NOPTS_VALUE))
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
    if (result)
    {
        if (result->dataType != QnAbstractMediaData::EMPTY_DATA)
            m_lastMediaFlags = result->flags;
        m_frameCnt++;
    }

    m_lastReceivedTime = qnSyncTime->currentMSecsSinceEpoch();
    return result;
}

void QnRtspClientArchiveDelegate::setAdditionalAttribute(const QByteArray& name, const QByteArray& value)
{
    m_rtspSession->setAdditionAttribute(name, value);
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
        m_rtspSession->setAdditionAttribute("x-no-find-iframe", "1");

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
                    if (m_rtspSession->getScale() > 0)
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
        m_rtspSession->sendPlay(time, endTime, m_rtspSession->getScale());
        m_rtspSession->removeAdditionAttribute("x-no-find-iframe");
    }
    m_sendedCSec = m_rtspSession->lastSendedCSeq();
    return time;
}

int QnRtspClientArchiveDelegate::getSequence() const
{
    return m_sendedCSec;
}

void QnRtspClientArchiveDelegate::setSingleshotMode(bool value)
{
    if (value == m_singleShotMode)
        return;

    m_singleShotMode = value;
    /*
    if (value)
        m_rtspSession->sendPause();
    else
        m_rtspSession->sendPlay(AV_NOPTS_VALUE, AV_NOPTS_VALUE, m_rtspSession->getScale());
    */
}

QnConstResourceVideoLayoutPtr QnRtspClientArchiveDelegate::getVideoLayout()
{
    if (m_customVideoLayout)
        return m_customVideoLayout;
    else if (m_camera && m_camera->resourcePool())
        return m_camera->getVideoLayout();
    else
        return QnMediaResource::getDefaultVideoLayout();
}

QnConstResourceAudioLayoutPtr QnRtspClientArchiveDelegate::getAudioLayout()
{
    if (!m_audioLayout) {
        m_audioLayout.reset( new QnResourceCustomAudioLayout() );
        for (QMap<int, QnNxRtpParserPtr>::const_iterator itr = m_parsers.begin(); itr != m_parsers.end(); ++itr)
        {
            QnConstMediaContextPtr context = itr.value()->mediaContext();
            if (context && context->getCodecType() == AVMEDIA_TYPE_AUDIO)
            {
                m_audioLayout->addAudioTrack(QnResourceAudioLayout::AudioTrack(
                    context, context->getAudioCodecDescription()));
            }
        }
    }
    return m_audioLayout;
}

void QnRtspClientArchiveDelegate::processMetadata(const quint8* data, int dataSize)
{
   // RtpHeader* rtpHeader = (RtpHeader*) data;
    const quint8* payload = data + RtpHeader::RTP_HEADER_SIZE;
    QByteArray ba((const char*)payload, data + dataSize - payload);
    if (ba.startsWith("clock="))
        m_rtspSession->parseRangeHeader(QLatin1String(ba));
    else if (ba.startsWith("drop-report"))
        emit dataDropped(m_reader);
}

namespace {

/**
 * @return Zero version if serverString is invalid.
 */
nx::utils::SoftwareVersion extractServerVersion(const nx_http::StringType& serverString)
{
    int versionStartPos = serverString.indexOf("/") + 1;
    int versionEndPos = serverString.indexOf(" ", versionStartPos);

    return nx::utils::SoftwareVersion(
        serverString.mid(versionStartPos, versionEndPos - versionStartPos));
}

} // namespace

QnAbstractDataPacketPtr QnRtspClientArchiveDelegate::processFFmpegRtpPayload(quint8* data, int dataSize, int channelNum, qint64* parserPosition)
{
    QnMutexLocker lock( &m_mutex );

    QnAbstractMediaDataPtr result;

    QMap<int, QnNxRtpParserPtr>::iterator itr = m_parsers.find(channelNum);
    if (itr == m_parsers.end())
    {
        auto parser = new QnNxRtpParser();
        // TODO: Use nx_http::header::Server here
        // to get RFC2616-conformant Server header parsing function.
        auto serverVersion = extractServerVersion(m_rtspSession->serverInfo());
        if (!serverVersion.isNull() && serverVersion < nx::utils::SoftwareVersion(3, 0))
            parser->setAudioEnabled(false);
        itr = m_parsers.insert(channelNum, QnNxRtpParserPtr(parser));
    }
    QnNxRtpParserPtr parser = itr.value();
    bool gotData = false;
    parser->processData(data, 0, dataSize, QnRtspStatistic(), gotData);
    *parserPosition = parser->position();
    if (gotData) {
        result = parser->nextData();
        if (result)
            result->channelNumber = channelNum;
    }
    return result;
}

void QnRtspClientArchiveDelegate::setSpeed(qint64 displayTime, double value)
{
    if (value == 0.0)
        return;

    m_position = displayTime;

    bool oldReverseMode = m_rtspSession->getScale() < 0;
    bool newReverseMode = value < 0;
    m_rtspSession->setScale(value);

    bool needSendRequest = !m_opened || oldReverseMode != newReverseMode ||  m_camera->isDtsBased();
    if (!needSendRequest)
        return;
    
    bool fromLive = newReverseMode && m_position == DATETIME_NOW;
    m_blockReopening = false;

    seek(displayTime, /* findIFrame */ true);
    if (fromLive)
        m_position = AV_NOPTS_VALUE;
}

bool QnRtspClientArchiveDelegate::isRealTimeSource() const
{
    if (m_lastMediaFlags == -1)
        return m_position == DATETIME_NOW;
    else
        return m_lastMediaFlags & QnAbstractMediaData::MediaFlags_LIVE;
}

bool QnRtspClientArchiveDelegate::setQuality(MediaQuality quality, bool fastSwitch, const QSize& resolution)
{
    if (m_quality == quality && fastSwitch <= m_qualityFastSwitch && m_resolution == resolution)
        return false;
    m_quality = quality;
    m_qualityFastSwitch = fastSwitch;
    m_resolution = resolution;

    if (m_quality == MEDIA_Quality_CustomResolution)
    {
        m_rtspSession->setAdditionAttribute(kResolutionParamName, resolutionToString(m_resolution).toLatin1());
        m_rtspSession->removeAdditionAttribute(kMediaQualityParamName);

        if (!fastSwitch)
            m_rtspSession->sendSetParameter(kMediaQualityParamName, resolutionToString(m_resolution).toLatin1());
    }
    else {
        QByteArray value = QnLexical::serialized(quality).toUtf8();

        m_rtspSession->setAdditionAttribute(kMediaQualityParamName, value);
        m_rtspSession->removeAdditionAttribute(kResolutionParamName);

        if (!fastSwitch)
            m_rtspSession->sendSetParameter(kMediaQualityParamName, value);
    }

    if (!m_rtspSession->isOpened())
        return false;

    if (!fastSwitch)
        return false; // switch quality without seek (it is take more time)
    else
        return true; // need send seek command
}

void QnRtspClientArchiveDelegate::setSendMotion(bool value)
{
    m_rtspSession->setAdditionAttribute("x-send-motion", value ? "1" : "0");
    m_rtspSession->sendSetParameter("x-send-motion", value ? "1" : "0");
}

void QnRtspClientArchiveDelegate::setMotionRegion(const QRegion& region)
{
    if (region.isEmpty())
    {
        m_rtspSession->removeAdditionAttribute("x-motion-region");
    }
    else
    {
        QBuffer buffer;
        buffer.open(QIODevice::WriteOnly);
        QDataStream stream(&buffer);
        stream << region;
        buffer.close();
        QByteArray s = buffer.data().toBase64();
        m_rtspSession->setAdditionAttribute("x-motion-region", s);
    }
    //m_rtspSession->sendPlay(AV_NOPTS_VALUE, AV_NOPTS_VALUE, m_rtspSession->getScale());
}

void QnRtspClientArchiveDelegate::beforeSeek(qint64 time)
{
    if (m_camera && m_camera->isGroupPlayOnly())
        return; // avoid close/open for VMAX

    qint64 diff = qAbs(m_lastReceivedTime - qnSyncTime->currentMSecsSinceEpoch());
    bool longNoData = ((m_position == DATETIME_NOW || time == DATETIME_NOW) && diff > 250) || diff > 1000*10;
    if (longNoData || m_quality == MEDIA_Quality_Low || m_quality == MEDIA_Quality_LowIframesOnly)
    {
        m_blockReopening = true;
        close();
    }
}

void QnRtspClientArchiveDelegate::beforeChangeSpeed(double speed)
{
    bool oldReverseMode = m_rtspSession->getScale() < 0;
    bool newReverseMode = speed < 0;

    // Reconnect If camera is offline and it is switch from live to archive
    if (oldReverseMode != newReverseMode && m_position == DATETIME_NOW)
    {
        if (newReverseMode)
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

void QnRtspClientArchiveDelegate::setupRtspSession(const QnSecurityCamResourcePtr &camera, const QnMediaServerResourcePtr &server, QnRtspClient* session, bool usePredefinedTracks) const {
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
    session->setAuth(auth, nx_http::header::AuthScheme::digest);

    if (!m_auth.videowall.isNull())
        session->setAdditionAttribute(Qn::VIDEOWALL_GUID_HEADER_NAME, m_auth.videowall.toString().toUtf8());
    session->setAdditionAttribute(Qn::EC2_RUNTIME_GUID_HEADER_NAME, camera->commonModule()->runningInstanceGUID().toByteArray());
    session->setAdditionAttribute(Qn::EC2_INTERNAL_RTP_FORMAT, "1" );
    session->setAdditionAttribute(Qn::CUSTOM_USERNAME_HEADER_NAME, m_auth.username.toUtf8());

    /* We can get here while client is already closing. */
    if (server)
    {
        QnNetworkProxyFactory factory(server->commonModule());
        QNetworkProxy proxy = factory.proxyToResource(server);
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
        m_rtspSession->setUsePredefinedTracks(0);
}

bool QnRtspClientArchiveDelegate::hasVideo() const
{
    return m_camera && m_camera->hasVideo(nullptr);
}
