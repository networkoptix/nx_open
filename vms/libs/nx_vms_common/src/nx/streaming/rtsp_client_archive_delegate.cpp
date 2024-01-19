// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rtsp_client_archive_delegate.h"

#include <QtConcurrent/QtConcurrentFilter>
#include <QtCore/QBuffer>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkCookie>

#include <api/network_proxy_factory.h>
#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_media_layout.h>
#include <core/resource_management/resource_pool.h>
#include <nx/media/av_codec_helper.h>
#include <nx/media/codec_parameters.h>
#include <nx/media/media_data_packet.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/rtsp/rtsp_types.h>
#include <nx/reflect/string_conversion.h>
#include <nx/rtp/rtp.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/streaming/rtp/parsers/nx_rtp_parser.h>
#include <nx/streaming/rtsp_client.h>
#include <nx/utils/suppress_exceptions.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <utils/common/sleep.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>

namespace {

static constexpr int kMaxRtpBufferSize = 65536;
static constexpr int kReopenTimeoutMs = 1000;

static const QByteArray kMediaQualityParamName = "x-media-quality";
static const QByteArray kResolutionParamName = "x-resolution";
static const QByteArray kSpeedParamName = "x-speed";
static const QByteArray kNoFindIframeParamName = "x-no-find-iframe";
static const QByteArray kMotionRegionParamName = "x-motion-region";
static const QByteArray kMediaStepParamName = "x-media-step";

static const QString kFfmpegCodecName = "ffmpeg";
static const QString kFfmpegMetadataCodecName = "ffmpeg-metadata";

QString resolutionToString(const QSize& size)
{
    if (size.width() > 0)
        return QString("%1x%2").arg(size.width()).arg(size.height());
    else
        return QString("%1p").arg(size.height());
}

bool isSpecialTimeValue(qint64 value)
{
    return value == DATETIME_NOW || value == DATETIME_INVALID;
}

QString getUrl(const QnSecurityCamResourcePtr& camera, const QnMediaServerResourcePtr& _server)
{
    // If camera is null we can't get its parent in any case.
    auto server = (_server || !camera) ? _server : camera->getParentServer();
    if (!server)
        return QString();

    QString url = server->rtspUrl() + '/';
    if (camera)
        url += camera->getPhysicalId();
    else
        url += server->rtspUrl();
    return url;
}

/**
 * @return Zero version if serverString is invalid.
 */
nx::utils::SoftwareVersion extractServerVersion(const nx::String& serverString)
{
    int versionStartPos = serverString.indexOf("/") + 1;
    int versionEndPos = serverString.indexOf(" ", versionStartPos);

    return nx::utils::SoftwareVersion(
        serverString.mid(versionStartPos, versionEndPos - versionStartPos));
}

struct ArchiveTimeCheckInfo
{
    ArchiveTimeCheckInfo(
        const QnSecurityCamResourcePtr& camera,
        const QnMediaServerResourcePtr& server,
        qint64* result):
        camera(camera), server(server), result(result)
    {
    }

    QnSecurityCamResourcePtr camera;
    QnMediaServerResourcePtr server;
    qint64* result = nullptr;
};

} // namespace

QnRtspClientArchiveDelegate::QnRtspClientArchiveDelegate(
    QnArchiveStreamReader* reader,
    nx::network::http::Credentials credentials,
    const QString& rtpLogTag,
    bool sleepIfEmptySocket)
    :
    QnAbstractArchiveDelegate(),
    m_rtspSession(new QnRtspClient(
        QnRtspClient::Config{
            .shouldGuessAuthDigest = true,
            .backChannelAudioOnly = false,
            .disableKeepAlive = true,
            .sleepIfEmptySocket = sleepIfEmptySocket})),
    m_reader(reader),
    m_credentials(std::move(credentials)),
    m_rtpLogTag(rtpLogTag),
    m_sleepIfEmptySocket(sleepIfEmptySocket)
{
    m_rtspSession->setPlayNowModeAllowed(true); //< Default value.
    m_footageUpToDate.test_and_set();
    m_currentServerUpToDate.test_and_set();
    m_rtpDataBuffer = new quint8[kMaxRtpBufferSize];
    m_flags |= Flag_SlowSource;
    m_flags |= Flag_CanProcessNegativeSpeed;
    m_flags |= Flag_CanProcessMediaStep;
    m_flags |= Flag_CanSendMetadata;
    m_flags |= Flag_CanSeekImmediatly;
    m_flags |= Flag_CanOfflineHasVideo;

    // These signals are emitted from the same thread. It is safe to call close();
    auto closeIfExpired =
        [this]()
        {
            if (isConnectionExpired())
                close();
        };

    if (reader)
    {
        connect(reader, &QnLongRunnable::paused, this, closeIfExpired, Qt::DirectConnection);
        connect(reader,
            &QnArchiveStreamReader::waitForDataCanBeAccepted,
            this,
            closeIfExpired,
            Qt::DirectConnection);
    }
}

void QnRtspClientArchiveDelegate::setCamera(const QnSecurityCamResourcePtr& camera)
{
    if (m_camera == camera)
        return;

    if (m_camera && m_camera->systemContext())
        m_camera->systemContext()->cameraHistoryPool()->disconnect(this);

    m_camera = camera;

    if (!NX_ASSERT(m_camera && m_camera->systemContext()))
        return;

    m_server = camera->getParentServer();

    auto context = camera->systemContext();
    auto maxSessionDuration = context->globalSettings()->maxRtspConnectDuration();
    if (maxSessionDuration.count() > 0)
        m_maxSessionDurationMs = maxSessionDuration;
    else
        m_maxSessionDurationMs = std::chrono::milliseconds::max();

    connect(context->cameraHistoryPool(),
        &QnCameraHistoryPool::cameraHistoryChanged,
        this,
        [this](const QnSecurityCamResourcePtr& camera)
        {
            /* Ignore other cameras changes. */
            if (camera != m_camera)
                return;

            if (m_server != getServerOnTime(m_position))
                m_currentServerUpToDate.clear();
        });

    connect(context->cameraHistoryPool(),
        &QnCameraHistoryPool::cameraFootageChanged,
        this,
        [this](const QnSecurityCamResourcePtr& camera)
        {
            /* Ignore other cameras changes. */
            if (camera != m_camera)
                return;
            m_footageUpToDate.clear();
        });

    setupRtspSession(camera, m_server, m_rtspSession.get());
}

void QnRtspClientArchiveDelegate::invalidateServer()
{
    m_currentServerUpToDate.clear();
}

void QnRtspClientArchiveDelegate::updateCredentials(
    const nx::network::http::Credentials& credentials)
{
    m_credentials = credentials;
    setupRtspSession(m_camera, m_server, m_rtspSession.get());
}

QnRtspClientArchiveDelegate::~QnRtspClientArchiveDelegate()
{
    close();
    delete[] m_rtpDataBuffer;
}

QnMediaServerResourcePtr QnRtspClientArchiveDelegate::getNextMediaServerFromTime(
    const QnSecurityCamResourcePtr& camera, qint64 time)
{
    if (!camera)
        return QnMediaServerResourcePtr();

    return camera->systemContext()->cameraHistoryPool()->getNextMediaServerAndPeriodOnTime(
        camera,
        time,
        /*searchForward*/ m_rtspSession->getScale() >= 0,
        &m_serverTimePeriod);
}

void QnRtspClientArchiveDelegate::checkGlobalTimeAsync(
    const QnSecurityCamResourcePtr& camera, const QnMediaServerResourcePtr& server, qint64* result)
{
    QnRtspClient client(
        QnRtspClient::Config{
            .shouldGuessAuthDigest = true,
            .backChannelAudioOnly = false,
            .sleepIfEmptySocket = m_sleepIfEmptySocket});
    QnRtspClientArchiveDelegate::setupRtspSession(camera, server, &client);
    const auto url = getUrl(camera, server);
    const auto error = client.open(url).errorCode;
    if (error != CameraDiagnostics::ErrorCode::noError)
    {
        NX_DEBUG(this,
            "%1(%2, %3) failed to open RTSP session to %4 with error %5",
            __func__,
            camera,
            server,
            url,
            error);
        return;
    }

    qint64 startTime = client.startTime();
    if (startTime != DATETIME_INVALID && startTime != DATETIME_NOW)
    {
        if (startTime < *result || *result == DATETIME_INVALID)
            *result = startTime;
    }
}

void QnRtspClientArchiveDelegate::checkMinTimeFromOtherServer(
    const QnSecurityCamResourcePtr& camera)
{
    if (!camera || !camera->resourcePool())
    {
        m_globalMinArchiveTime = DATETIME_INVALID;
        return;
    }

    QnMediaServerResourceList mediaServerList =
        camera->systemContext()->cameraHistoryPool()->getCameraFootageData(camera, true);

    /* Check if no archive available on any server. */
    /* Or check if archive belong to the current server only */
    if (mediaServerList.isEmpty()
        || (mediaServerList.size() == 1 && mediaServerList[0] == camera->getParentServer()))
    {
        m_globalMinArchiveTime = DATETIME_INVALID;
        return;
    }

    QList<ArchiveTimeCheckInfo> checkList;
    qint64 currentMinTime = DATETIME_INVALID;
    qint64 otherMinTime = DATETIME_INVALID;
    for (const auto& server: mediaServerList)
    {
        checkList << ArchiveTimeCheckInfo(
            camera, server, server == m_server ? &currentMinTime : &otherMinTime);
    }
    QtConcurrent::blockingFilter(checkList,
        nx::suppressExceptions(
            [this](const ArchiveTimeCheckInfo& checkInfo)
            {
                checkGlobalTimeAsync(checkInfo.camera, checkInfo.server, checkInfo.result);
                return true;
            },
            this,
            [](auto item)
            {
                return NX_FMT("checkGlobalTimeAsync(%1, %2) opening RTSP session to %3",
                    item.camera,
                    item.server,
                    nx::suppressExceptions(&getUrl)(item.camera, item.server));
            }));
    if ((otherMinTime != DATETIME_INVALID)
        && (currentMinTime == DATETIME_INVALID || otherMinTime < currentMinTime))
        m_globalMinArchiveTime = otherMinTime;
    else
        m_globalMinArchiveTime = DATETIME_INVALID;
}

QnMediaServerResourcePtr QnRtspClientArchiveDelegate::getServerOnTime(qint64 timeUsec)
{
    if (!m_camera)
        return QnMediaServerResourcePtr();
    QnMediaServerResourcePtr currentServer = m_camera->getParentServer();

    if (timeUsec == DATETIME_NOW)
        return currentServer;

    qint64 timeMs = timeUsec / 1000;

    QnMediaServerResourcePtr mediaServer =
        m_camera->systemContext()->cameraHistoryPool()->getMediaServerOnTime(
            m_camera, timeMs, &m_serverTimePeriod);

    if (!mediaServer)
        return currentServer;

    if (mediaServer != m_server)
        NX_DEBUG(this, "Switch to server %1", mediaServer->getUrl());

    return mediaServer;
}

bool QnRtspClientArchiveDelegate::open(
    const QnResourcePtr& resource,
    AbstractArchiveIntegrityWatcher* /*archiveIntegrityWatcher*/)
{
    const auto camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!NX_ASSERT(camera))
        return false;

    setCamera(camera);

    bool result = openInternal();
    if (!result)
    {
        for (int i = 0; i < kReopenTimeoutMs / 10 && !m_closing; ++i)
            QnSleep::msleep(10);
    }
    return result;
}

bool QnRtspClientArchiveDelegate::openInternal()
{
    if (m_rtspSession->isOpened())
        return true;

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_parsers.clear();
    }
    m_frameCnt = 0;
    m_closing = false;
    m_lastMediaFlags = -1;
    m_sessionTimeout.invalidate();
    setCustomVideoLayout(QnCustomResourceVideoLayoutPtr());

    // Force current value to avoid flicker effect while current server is being changed.
    m_globalMinArchiveTime = startTime();

    m_server = getServerOnTime(m_position); //< Try to update server.
    if (!m_server || m_server->getStatus() == nx::vms::api::ResourceStatus::offline)
    {
        if (isSpecialTimeValue(m_globalMinArchiveTime))
            checkMinTimeFromOtherServer(m_camera);
        return false;
    }

    if (m_playNowModeAllowed)
    {
        m_channelCount = 1;
        QnConstResourceVideoLayoutPtr videoLayout = m_camera->getVideoLayout(0);
        if (videoLayout)
            m_channelCount = videoLayout->channelCount();
    }
    setupRtspSession(m_camera, m_server, m_rtspSession.get());

    const bool isOpened = m_rtspSession->open(getUrl(m_camera, m_server), m_lastSeekTime).errorCode
        == CameraDiagnostics::ErrorCode::noError;
    if (isOpened)
    {
        qint64 endTime = m_position;
        if (m_forcedEndTime)
            endTime = m_forcedEndTime;

        m_rtspSession->play(m_position, endTime, m_rtspSession->getScale());

        checkMinTimeFromOtherServer(m_camera);

        m_rtpData = nullptr;
        if (m_playNowModeAllowed)
        {
            // TODO: #dmishin Temporary solution.
            m_rtspDevice = std::make_unique<QnRtspIoDevice>(
                m_rtspSession.get(), nx::vms::api::RtpTransportType::tcp);

            m_rtpData = m_rtspDevice.get();
        }
        else
        {
            const auto& trackInfo = m_rtspSession->getTrackInfo();
            if (!trackInfo.empty())
                m_rtpData = trackInfo[0].ioDevice.get();
        }
        if (!m_rtpData)
            m_rtspSession->stop();
    }
    else
    {
        m_rtspSession->stop();
    }
    m_sendedCSec = m_rtspSession->lastSendedCSeq();

    if (isOpened)
    {
        m_sessionTimeout.restart();

        QStringList audioSDP = m_rtspSession->getSdpByType(nx::rtp::Sdp::MediaType::Audio);
        parseAudioSDP(audioSDP);

        QString vLayout = m_rtspSession->getVideoLayout();
        if (!vLayout.isEmpty())
        {
            auto newValue = QnCustomResourceVideoLayout::fromString(vLayout);
            bool isChanged = getVideoLayout()->toString() != newValue->toString();
            setCustomVideoLayout(newValue);
            if (m_reader && isChanged)
                emit m_reader->videoLayoutChanged();
        }
    }
    return isOpened;
}

void QnRtspClientArchiveDelegate::parseAudioSDP(const QStringList& audioSDP)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    for (int i = 0; i < audioSDP.size(); ++i)
    {
        if (audioSDP[i].startsWith("a=fmtp"))
        {
            int configPos = audioSDP[i].indexOf("config=");
            if (configPos > 0)
            {
                auto data = QByteArray::fromBase64(audioSDP[i].mid(configPos + 7).toUtf8());
                auto context = std::make_shared<CodecParameters>();
                if (!context->deserialize(data.data(), data.size()))
                {
                    NX_DEBUG(this, "Failed to deserialize codec parameters");
                    continue;
                }
                if (context && context->getCodecType() == AVMEDIA_TYPE_AUDIO)
                    m_audioLayout.reset(new AudioLayout(context));
            }
        }
    }
}

void QnRtspClientArchiveDelegate::beforeClose()
{
    m_closing = true;
    m_rtspSession->shutdown();
}

void QnRtspClientArchiveDelegate::close()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_rtspSession->stop();
}

qint64 QnRtspClientArchiveDelegate::startTime() const
{
    qint64 result = m_globalMinArchiveTime;
    if (result == DATETIME_INVALID)
        result = m_rtspSession->startTime();

    if (result == DATETIME_NOW || result <= qnSyncTime->currentUSecsSinceEpoch())
        return result;
    else
        return DATETIME_NOW; // archive in a future
}

qint64 QnRtspClientArchiveDelegate::endTime() const
{
    // LIVE or archive future point as right edge for the server video.
    return DATETIME_NOW;
}

bool QnRtspClientArchiveDelegate::reopen()
{
    close();

    if (m_blockReopening)
        return false;

    if (m_reopenTimer.isValid() && m_reopenTimer.elapsed() < kReopenTimeoutMs)
    {
        for (int i = 0; i < (kReopenTimeoutMs - m_reopenTimer.elapsed()) / 10 && !m_closing; ++i)
            QnSleep::msleep(10);
    }
    m_reopenTimer.restart();

    if (m_camera)
        return openInternal();

    return false;
}

bool QnRtspClientArchiveDelegate::isConnectionExpired() const
{
    return m_sessionTimeout.isValid()
        && m_sessionTimeout.hasExpired(m_maxSessionDurationMs.count());
}

QnAbstractMediaDataPtr QnRtspClientArchiveDelegate::getNextData()
{
    if (!m_currentServerUpToDate.test_and_set() || isConnectionExpired())
        reopen();

    if (!m_footageUpToDate.test_and_set())
        checkMinTimeFromOtherServer(m_camera);

    QnAbstractMediaDataPtr result = getNextDataInternal();
    if (!result && !m_blockReopening && !m_closing)
        result = getNextDataInternal(); //< Try again in case of RTSP reconnect.

    if (!m_serverTimePeriod.isNull())
    {
        // Check if archive is moved to other video server.
        qint64 timeMs = DATETIME_INVALID;
        if (result && result->timestamp >= 0)
            timeMs = result->timestamp / 1000;

        // Do not switch server in case of DATETIME_INVALID and any other invalid packet timings.
        bool outOfRange = timeMs != DATETIME_INVALID
            && ((m_rtspSession->getScale() >= 0 && timeMs >= m_serverTimePeriod.endTimeMs())
                || (m_rtspSession->getScale() < 0 && timeMs < m_serverTimePeriod.startTimeMs));
        if (result == 0 || outOfRange || result->dataType == QnAbstractMediaData::EMPTY_DATA)
        {
            if (m_lastSeekTime == DATETIME_INVALID)
                m_lastSeekTime = qnSyncTime->currentUSecsSinceEpoch();
            QnMediaServerResourcePtr newServer =
                getNextMediaServerFromTime(m_camera, m_lastSeekTime / 1000);
            if (newServer)
            {
                NX_DEBUG(this,
                    "Reached the edge for archive in a current server. "
                    "packetTime=%1, period: %2 - %3",
                    nx::utils::timestampToDebugString(timeMs),
                    nx::utils::timestampToDebugString(m_serverTimePeriod.startTimeMs),
                    nx::utils::timestampToDebugString(m_serverTimePeriod.endTimeMs()));

                m_server = newServer;
                m_lastSeekTime = m_serverTimePeriod.startTimeMs * 1000;
                if (m_rtspSession->getScale() > 0)
                    m_position = m_serverTimePeriod.startTimeMs * 1000;
                else
                    m_position = (m_serverTimePeriod.endTimeMs() - 1) * 1000;
                close();
                openInternal();

                result = getNextData();
                if (result)
                    result->flags |= QnAbstractMediaData::MediaFlags_NewServer;
            }
            else
            {
                m_serverTimePeriod.clear();
            }
        }
    }
    if (!result)
        m_lastError.errorCode = CameraDiagnostics::ErrorCode::serverTerminated;
    else
        m_lastError.errorCode = CameraDiagnostics::ErrorCode::noError;

    return result;
}

QnAbstractMediaDataPtr QnRtspClientArchiveDelegate::getNextDataInternal()
{
    // Sometimes function may return zero packet if no data arrived.
    QnAbstractMediaDataPtr result;
    QElapsedTimer receiveTimer;
    receiveTimer.start();
    while (!result)
    {
        if (!m_rtpData || (!m_rtspSession->isOpened() && !m_closing))
        {
            reopen();
            return result;
        }

        int rtpChannelNum = 0;
        int blockSize = m_rtpData->read((char*) m_rtpDataBuffer, kMaxRtpBufferSize);
        if (blockSize <= 0 && !m_closing)
        {
            reopen();
            return result;
        }

        quint8* data = m_rtpDataBuffer;
        if (m_tcpMode)
        {
            if (blockSize < 4)
                return result;

            rtpChannelNum = m_rtpDataBuffer[1];
            blockSize -= 4;
            data += 4;
        }
        else
        {
            rtpChannelNum = m_rtpData->getMediaSocket()->getLocalAddress().port;
        }

        QString codecName;
        if (m_playNowModeAllowed)
        {
            codecName = rtpChannelNum < (m_channelCount + 1) * 2
                ? kFfmpegCodecName
                : kFfmpegMetadataCodecName;
        }
        else
        {
            codecName = m_rtspSession->getTrackCodec(rtpChannelNum).toLower();
        }

        qint64 parserPosition = DATETIME_INVALID;
        if (codecName == kFfmpegCodecName)
        {
            auto [packet, success] =
                processFFmpegRtpPayload(data, blockSize, rtpChannelNum / 2, &parserPosition);
            result = std::dynamic_pointer_cast<QnAbstractMediaData>(packet);

            // If client can't receive first frame for too long, inform that stream is slow.
            if (!result && m_frameCnt == 0 && receiveTimer.elapsed() > 4000)
                emit dataDropped(m_reader);

            if (!success)
                return result;
        }
        else if (codecName == kFfmpegMetadataCodecName)
        {
            processMetadata(data, blockSize);
        }
        else
        {
            NX_WARNING(this, "Unsupported codec format '%1'", codecName);
        }

        if (result && m_sendedCSec != result->opaque)
            result.reset(); //< Ignore old archive data.
        if (result && parserPosition != DATETIME_INVALID)
            m_position = parserPosition;
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

void QnRtspClientArchiveDelegate::setAdditionalAttribute(
    const QByteArray& name,
    const QByteArray& value)
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
    NX_DEBUG(this,
        "Set position %1 for device %2",
        nx::utils::timestampToDebugString(time / 1000),
        m_camera);

    m_blockReopening = false;
    m_lastSeekTime = m_position = time;

    QnMediaServerResourcePtr newServer = getServerOnTime(m_position);
    if (newServer != m_server)
    {
        close();
        m_server = newServer;
    }

    if (!findIFrame)
        m_rtspSession->setAdditionAttribute(kNoFindIframeParamName, "1");

    if (!m_rtspSession->isOpened() && m_camera)
    {
        if (!openInternal())
        {
            // Try next server in the list immediately, It is improve seek time if current server
            // is offline and next server is exists.
            while (!m_closing)
            {
                QnMediaServerResourcePtr nextServer =
                    getNextMediaServerFromTime(m_camera, m_lastSeekTime / 1000);
                if (nextServer && nextServer != m_server)
                {
                    m_server = nextServer;
                    m_lastSeekTime = m_serverTimePeriod.startTimeMs * 1000;
                    if (m_rtspSession->getScale() > 0)
                        m_position = m_serverTimePeriod.startTimeMs * 1000;
                    else
                        m_position = (m_serverTimePeriod.endTimeMs() - 1) * 1000;
                    close();
                    if (openInternal())
                        break;
                }
                else
                {
                    break;
                }
            }
        }
    }
    else
    {
        qint64 endTime = DATETIME_INVALID;
        if (m_forcedEndTime)
            endTime = m_forcedEndTime;
        else if (m_singleShotMode)
            endTime = time;
        m_rtspSession->sendPlay(time, endTime, m_rtspSession->getScale());
        m_rtspSession->removeAdditionAttribute(kNoFindIframeParamName);
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
}

void QnRtspClientArchiveDelegate::setCustomVideoLayout(const QnCustomResourceVideoLayoutPtr& value)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_customVideoLayout = value;
}

QnConstResourceVideoLayoutPtr QnRtspClientArchiveDelegate::getVideoLayout()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_customVideoLayout)
        return m_customVideoLayout;

    if (m_camera && m_camera->resourcePool())
        return m_camera->getVideoLayout();

    return QnMediaResource::getDefaultVideoLayout();
}

AudioLayoutConstPtr QnRtspClientArchiveDelegate::getAudioLayout()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!m_audioLayout)
    {
        m_audioLayout.reset(new AudioLayout());
        for (auto itr = m_parsers.begin(); itr != m_parsers.end(); ++itr)
        {
            CodecParametersConstPtr context = itr.value()->mediaContext();
            if (context && context->getCodecType() == AVMEDIA_TYPE_AUDIO)
                m_audioLayout->addTrack(context, context->getAudioCodecDescription());
        }
    }
    return m_audioLayout;
}

void QnRtspClientArchiveDelegate::processMetadata(const quint8* data, int dataSize)
{
    const quint8* payload = data + nx::rtp::RtpHeader::kSize;
    QByteArray ba((const char*) payload, data + dataSize - payload);
    if (ba.startsWith("clock="))
        m_rtspSession->parseRangeHeader(QLatin1String(ba));
    else if (ba.startsWith("drop-report"))
        emit dataDropped(m_reader);
    else if (ba.startsWith(nx::network::rtsp::kUpdateHeader))
    {
        NX_VERBOSE(this, "Got need-update header");
        emit needUpdateTimeLine();
    }
}

std::pair<QnAbstractDataPacketPtr, bool> QnRtspClientArchiveDelegate::processFFmpegRtpPayload(
    quint8* data, int dataSize, int channelNum, qint64* parserPosition)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    QnAbstractMediaDataPtr result;

    auto itr = m_parsers.find(channelNum);
    if (itr == m_parsers.end())
    {
        auto parser = new nx::rtp::QnNxRtpParser(m_camera->getId(), m_rtpLogTag);
        // TODO: Use nx::network::http::header::Server here
        // to get RFC2616-conformant Server header parsing function.
        auto serverVersion = extractServerVersion(m_rtspSession->serverInfo());
        parser->setServerVersion(serverVersion);
        if (!serverVersion.isNull() && serverVersion < nx::utils::SoftwareVersion(3, 0))
            parser->setAudioEnabled(false);
        itr = m_parsers.insert(channelNum, nx::rtp::QnNxRtpParserPtr(parser));
    }
    nx::rtp::QnNxRtpParserPtr parser = itr.value();
    bool gotData = false;
    const auto parseResult = parser->processData(data, dataSize, gotData);
    if (!parseResult.success)
    {
        NX_DEBUG(this, "RTP parser error: %1", parseResult.message);
        return {
            QnAbstractDataPacketPtr(), parseResult.success}; //< Report error to reopen connection.
    }
    *parserPosition = parser->position();
    if (gotData)
    {
        result = parser->nextData();
        if (result)
            result->channelNumber = channelNum;
    }
    return {result, parseResult.success};
}

void QnRtspClientArchiveDelegate::setSpeed(qint64 displayTime, double value)
{
    // Don't seek stream on pause if not reverse play currently
    if (value == 0.0 && m_rtspSession->getScale() >= 0)
        return;

    m_position = displayTime;

    bool oldReverseMode = m_rtspSession->getScale() < 0;
    bool newReverseMode = value < 0;
    m_rtspSession->setScale(value);

    bool needSendSeek = !m_rtspSession->isOpened() || oldReverseMode != newReverseMode
        || m_camera->hasCameraCapabilities(
            nx::vms::api::DeviceCapability::isPlaybackSpeedSupported);
    if (!needSendSeek)
    {
        m_rtspSession->sendSetParameter(kSpeedParamName, QByteArray::number(value));
        return;
    }

    bool fromLive = newReverseMode && m_position == DATETIME_NOW;
    m_blockReopening = false;

    seek(displayTime, /* findIFrame */ true);
    if (fromLive)
        m_position = DATETIME_INVALID;
}

bool QnRtspClientArchiveDelegate::isRealTimeSource() const
{
    if (m_lastMediaFlags == -1)
        return m_position == DATETIME_NOW;
    else
        return m_lastMediaFlags & QnAbstractMediaData::MediaFlags_LIVE;
}

bool QnRtspClientArchiveDelegate::setQuality(
    MediaQuality quality, bool fastSwitch, const QSize& resolution)
{
    if (m_quality == quality && fastSwitch <= m_qualityFastSwitch && m_resolution == resolution)
        return false;
    m_quality = quality;
    m_qualityFastSwitch = fastSwitch;
    m_resolution = resolution;

    if (!isRealTimeSource() && m_camera
        && m_camera->hasCameraCapabilities(
            nx::vms::api::DeviceCapability::dualStreamingForLiveOnly))
    {
        return false;
    }

    if (m_quality == MEDIA_Quality_CustomResolution)
    {
        m_rtspSession->setAdditionAttribute(
            kResolutionParamName, resolutionToString(m_resolution).toLatin1());
        m_rtspSession->removeAdditionAttribute(kMediaQualityParamName);

        if (!fastSwitch)
            m_rtspSession->sendSetParameter(
                kMediaQualityParamName, resolutionToString(m_resolution).toLatin1());
    }
    else
    {
        const auto value = QByteArray::fromStdString(nx::reflect::toString(quality));

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

void QnRtspClientArchiveDelegate::setStreamDataFilter(nx::vms::api::StreamDataFilters filter)
{
    if (m_streamDataFilter == filter)
        return;
    m_streamDataFilter = filter;

    const auto value = QByteArray::fromStdString(nx::reflect::toString(filter));
    const auto sendMotionValue =
        filter.testFlag(nx::vms::api::StreamDataFilter::motion) ? "1" : "0";

    m_rtspSession->setAdditionAttribute(Qn::RTSP_DATA_FILTER_HEADER_NAME, value);
    m_rtspSession->setAdditionAttribute(Qn::RTSP_DATA_SEND_MOTION_HEADER_NAME, sendMotionValue);
    if (m_rtspSession->isOpened())
    {
        m_rtspSession->sendSetParameter(Qn::RTSP_DATA_FILTER_HEADER_NAME, value);
        m_rtspSession->sendSetParameter(Qn::RTSP_DATA_SEND_MOTION_HEADER_NAME, sendMotionValue);
    }
}

void QnRtspClientArchiveDelegate::setStorageLocationFilter(nx::vms::api::StorageLocation filter)
{
    m_storageLocationFilter = filter;
    const auto value = QByteArray::fromStdString(nx::reflect::toString(filter));
    m_rtspSession->setAdditionAttribute(Qn::RTSP_DATA_CHUNKS_FILTER_HEADER_NAME, value);
    if (m_rtspSession->isOpened())
        m_rtspSession->sendSetParameter(Qn::RTSP_DATA_CHUNKS_FILTER_HEADER_NAME, value);
}

nx::vms::api::StreamDataFilters QnRtspClientArchiveDelegate::streamDataFilter() const
{
    return m_streamDataFilter;
}

void QnRtspClientArchiveDelegate::setMotionRegion(const QRegion& region)
{
    if (region.isEmpty())
    {
        m_rtspSession->removeAdditionAttribute(kMotionRegionParamName);
    }
    else
    {
        QBuffer buffer;
        buffer.open(QIODevice::WriteOnly);
        QDataStream stream(&buffer);
        stream << region;
        buffer.close();
        QByteArray s = buffer.data().toBase64();
        m_rtspSession->setAdditionAttribute(kMotionRegionParamName, s);
    }
}

void QnRtspClientArchiveDelegate::beforeSeek(qint64 time)
{
    if (m_camera
        && (m_camera->isGroupPlayOnly()
            || m_camera->hasCameraCapabilities(
                nx::vms::api::DeviceCapability::synchronousChannel)))
    {
        return; // avoid close/open for NVR
    }

    qint64 diff = qAbs(m_lastReceivedTime - qnSyncTime->currentMSecsSinceEpoch());
    bool longNoData =
        ((m_position == DATETIME_NOW || time == DATETIME_NOW) && diff > 250) || diff > 1000 * 10;
    if (longNoData || m_quality == MEDIA_Quality_Low || m_quality == MEDIA_Quality_LowIframesOnly)
    {
        beforeClose();
    }
}

void QnRtspClientArchiveDelegate::beforeChangeSpeed(double speed)
{
    bool oldReverseMode = m_rtspSession->getScale() < 0;
    bool newReverseMode = speed < 0;

    // Reconnect If camera is offline and it is switch from live to archive.
    if (oldReverseMode != newReverseMode && m_position == DATETIME_NOW)
    {
        if (newReverseMode)
            beforeSeek(DATETIME_INVALID);
        else
            m_blockReopening = false;
    }
}

void QnRtspClientArchiveDelegate::setRange(qint64 startTime, qint64 endTime, qint64 frameStep)
{
    NX_VERBOSE(this,
        "Set range %1 - %2, framestamp %3 us",
        nx::utils::timestampToDebugString(startTime),
        nx::utils::timestampToDebugString(endTime),
        frameStep);

    setAdditionalAttribute(kMediaStepParamName, QByteArray::number(frameStep));
    seek(startTime, endTime);
}

void QnRtspClientArchiveDelegate::setupRtspSession(
    const QnSecurityCamResourcePtr& camera,
    const QnMediaServerResourcePtr& server,
    QnRtspClient* session) const
{
    session->setCredentials(m_credentials, nx::network::http::header::AuthScheme::digest);
    session->setAdditionAttribute(
        Qn::EC2_RUNTIME_GUID_HEADER_NAME, camera->systemContext()->sessionId().toByteArray());
    session->setAdditionAttribute(Qn::EC2_INTERNAL_RTP_FORMAT, "1");
    session->setAdditionAttribute(
        Qn::CUSTOM_USERNAME_HEADER_NAME, QString::fromStdString(m_credentials.username).toUtf8());

    // We can get here while client is already closing.
    if (server)
    {
        QNetworkProxy proxy = QnNetworkProxyFactory::proxyToResource(server);
        if (proxy.type() != QNetworkProxy::NoProxy)
            session->setProxyAddr(proxy.hostName(), proxy.port());

        session->setAdditionAttribute(
            Qn::SERVER_GUID_HEADER_NAME,
            server->getId().toByteArray());
    }

    session->setTransport(nx::vms::api::RtpTransportType::tcp);
    const auto settings = m_camera->systemContext()->globalSettings();
    session->setTcpRecvBufferSize(settings->mediaBufferSizeKb() * 1024);
}

void QnRtspClientArchiveDelegate::setPlayNowModeAllowed(bool value)
{
    m_playNowModeAllowed = value;
    m_rtspSession->setPlayNowModeAllowed(value);
}

int QnRtspClientArchiveDelegate::protocol() const
{
    int result = 0;
    if (m_rtspDevice)
    {
        if (const auto socket = m_rtspDevice->getMediaSocket())
            socket->getProtocol(&result);
    }
    return result;
}

bool QnRtspClientArchiveDelegate::hasVideo() const
{
    return m_camera && m_camera->hasVideo(nullptr);
}

void QnRtspClientArchiveDelegate::pleaseStop()
{
    // TODO: #vbreus It should be done atomically under lock. To be fixed in 5.1 since there is no
    // way to do it right without significant changes.
    m_blockReopening = true;
    if (m_rtspSession->isOpened())
        m_rtspSession->shutdown();
}

void QnRtspClientArchiveDelegate::setMediaRole(PlaybackMode mode)
{
    setAdditionalAttribute(Qn::EC2_MEDIA_ROLE, nx::reflect::toString(mode).data());
}

CameraDiagnostics::Result QnRtspClientArchiveDelegate::lastError() const
{
    return m_lastError;
}
