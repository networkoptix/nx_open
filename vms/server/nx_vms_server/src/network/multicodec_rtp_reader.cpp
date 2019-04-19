#include "multicodec_rtp_reader.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QSettings>
#include <set>

#include <nx/fusion/serialization/lexical.h>
#include <nx/vms/api/types/rtp_types.h>

#include <nx/vms/event/events/reasoned_event.h>
#include <nx/vms/event/events/network_issue_event.h>

#include <common/common_module.h>

#include <api/global_settings.h>
#include <api/app_server_connection.h>

#include <nx/streaming/rtp/rtp.h>
#include <nx/streaming/rtp/onvif_header_extension.h>
#include <nx/streaming/rtp/parsers/h264_rtp_parser.h>
#include <nx/streaming/rtp/parsers/hevc_rtp_parser.h>
#include <nx/streaming/rtp/parsers/aac_rtp_parser.h>
#include <nx/streaming/rtp/parsers/simpleaudio_rtp_parser.h>
#include <nx/streaming/rtp/parsers/mjpeg_rtp_parser.h>

#include <nx/network/compat_poll.h>
#include <nx/utils/log/log.h>

#include <utils/common/synctime.h>
#include <utils/common/util.h>

#include <core/resource/network_resource.h>
#include <core/resource/resource_media_layout.h>
#include <core/resource/media_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_data_pool.h>

#include <nx/utils/scope_guard.h>
#include "nx/network/rtsp/rtsp_types.h"

#include <nx/vms/server/resource/camera.h>
#include <nx/vms/server/network/multicast_address_registry.h>

using namespace nx;

namespace {

static const int RTSP_RETRY_COUNT = 6;
static const int RTCP_REPORT_TIMEOUT = 30 * 1000;

static const int MAX_MEDIA_SOCKET_COUNT = 5;
static const int MEDIA_DATA_READ_TIMEOUT_MS = 100;
static const std::chrono::minutes PACKET_LOSS_WARNING_TIME(1);

// prefix has the following format $<ChannelId(1byte)><PayloadLength(2bytes)>
static const int kInterleavedRtpOverTcpPrefixLength = 4;

QString getConfiguredVideoLayout(const QnResourcePtr& resource)
{
    QString configuredLayout;
    auto secResource = resource.dynamicCast<QnSecurityCamResource>();
    if (secResource)
    {
        configuredLayout = secResource->resourceData().value<QString>(
            ResourceDataKey::kVideoLayout);
    }
    if (configuredLayout.isEmpty())
    {
        QnResourceTypePtr resType = qnResTypePool->getResourceType(resource->getTypeId());
        if (resType)
            configuredLayout = resType->defaultValue(ResourcePropertyKey::kVideoLayout);
    }
    return configuredLayout;
}

nx::streaming::rtp::TimePolicy getTimePolicy(const QnResourcePtr& res)
{
    auto secResource = res.dynamicCast<QnSecurityCamResource>();
    if (secResource && secResource->trustCameraTime())
        return nx::streaming::rtp::TimePolicy::useCameraTimeIfCorrect;

    return nx::streaming::rtp::TimePolicy::bindCameraTimeToLocalTime;
}

} // namespace

nx::utils::Mutex QnMulticodecRtpReader::s_defaultTransportMutex;
nx::vms::api::RtpTransportType QnMulticodecRtpReader::s_defaultTransportToUse =
    nx::vms::api::RtpTransportType::tcp;

QnMulticodecRtpReader::QnMulticodecRtpReader(
    const QnResourcePtr& res,
    const nx::streaming::rtp::TimeOffsetPtr& timeOffset,
    std::unique_ptr<nx::network::AbstractStreamSocket> tcpSock)
:
    QnResourceConsumer(res),
    m_RtpSession(QnRtspClient::Config(), std::move(tcpSock)),
    m_timeHelper(res->getUniqueId().toStdString(), timeOffset),
    m_pleaseStop(false),
    m_gotSomeFrame(false),
    m_role(Qn::CR_Default),
    m_gotData(false),
    m_rtpStarted(false),
    m_prefferedAuthScheme(nx::network::http::header::AuthScheme::digest),
    m_rtpTransport(nx::vms::api::RtpTransportType::automatic)
{
    const auto& globalSettings = res->commonModule()->globalSettings();
    m_rtpFrameTimeoutMs = globalSettings->rtpFrameTimeoutMs();
    m_maxRtpRetryCount = globalSettings->maxRtpRetryCount();

    m_RtpSession.setTCPTimeout(std::chrono::milliseconds(m_rtpFrameTimeoutMs));

    QnMediaResourcePtr mr = qSharedPointerDynamicCast<QnMediaResource>(res);
    m_numberOfVideoChannels = 1;
    m_customVideoLayout.clear();

    QnSecurityCamResourcePtr camRes = qSharedPointerDynamicCast<QnSecurityCamResource>(res);
    if (camRes)
        connect(this,       &QnMulticodecRtpReader::networkIssue, camRes.data(), &QnSecurityCamResource::networkIssue,              Qt::DirectConnection);
    Qn::directConnect(res.data(), &QnResource::propertyChanged, this, &QnMulticodecRtpReader::at_propertyChanged);

    m_timeHelper.setTimePolicy(getTimePolicy(m_resource));
}

QnMulticodecRtpReader::~QnMulticodecRtpReader()
{
    directDisconnectAll();

    for (unsigned int i = 0; i < m_demuxedData.size(); ++i)
        delete m_demuxedData[i];

    unregisterMulticastAddresses();
}

void QnMulticodecRtpReader::setRequest(const QString& request)
{
    m_request = request;
    calcStreamUrl();
}

void QnMulticodecRtpReader::clearKeyData(int channelNum)
{
    if (m_gotKeyDataInfo.size() > channelNum)
        m_gotKeyDataInfo[channelNum] = false;
}

bool QnMulticodecRtpReader::gotKeyData(const QnAbstractMediaDataPtr& mediaData)
{
    if ((size_t)m_gotKeyDataInfo.size() <= mediaData->channelNumber)
        m_gotKeyDataInfo.resize(mediaData->channelNumber + 1);
    if (mediaData->dataType == QnAbstractMediaData::VIDEO)
    {
        if (mediaData->flags & AV_PKT_FLAG_KEY)
            m_gotKeyDataInfo[mediaData->channelNumber] = true;
        return m_gotKeyDataInfo[mediaData->channelNumber];
    }
    return true;
}

QnAbstractMediaDataPtr QnMulticodecRtpReader::getNextData()
{
    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    const qint64 position = m_positionUsec.exchange(AV_NOPTS_VALUE);
    if (position != AV_NOPTS_VALUE)
    {
        m_RtpSession.sendPlay(position, AV_NOPTS_VALUE /*endTime */, m_RtpSession.getScale());
        createTrackParsers();
    }

    QnAbstractMediaDataPtr result;
    do {
        if (m_RtpSession.getActualTransport() == nx::vms::api::RtpTransportType::tcp)
            result = getNextDataTCP();
        else
            result = getNextDataUDP();

    } while(result && !gotKeyData(result));

    if (result) {
        m_gotSomeFrame = true;
        return result;
    }

    if (!m_gotSomeFrame)
        return result; // if no frame received yet do not report network issue error

    bool isRtpFail = !m_RtpSession.isOpened() && m_rtpStarted;
    int elapsed = m_dataTimer.elapsed();
    if (isRtpFail || elapsed > m_rtpFrameTimeoutMs)
    {
        QString reasonParamsEncoded;
        vms::api::EventReason reason;
        if (elapsed > m_rtpFrameTimeoutMs) {
            reason = vms::api::EventReason::networkNoFrame;
            reasonParamsEncoded = vms::event::NetworkIssueEvent::encodeTimeoutMsecs(elapsed);
            NX_WARNING(this, "RTP read timeout for camera %1. Reopen stream", getResource()->getUniqueId());
        }
        else {
            reason = vms::api::EventReason::networkConnectionClosed;
            reasonParamsEncoded = vms::event::NetworkIssueEvent::encodePrimaryStream(m_role != Qn::CR_SecondaryLiveVideo);
            NX_WARNING(this, "RTP connection was forcibly closed by camera %1. Reopen stream", getResource()->getUniqueId());
        }

        emit networkIssue(getResource(),
            qnSyncTime->currentUSecsSinceEpoch(),
            reason,
            reasonParamsEncoded);
        QnVirtualCameraResource* cam = dynamic_cast<QnVirtualCameraResource*>(getResource().data());
        if (cam)
            cam->issueOccured();

    }

    return result;
}

void QnMulticodecRtpReader::processTcpRtcp(quint8* buffer, int bufferSize, int bufferCapacity)
{
    if (!m_RtpSession.processTcpRtcpData(buffer, bufferSize))
        NX_VERBOSE(this, "Can't parse RTCP report");
    int outBufSize = nx::streaming::rtp::buildClientRtcpReport(buffer+4, bufferCapacity-4);
    if (outBufSize > 0)
    {
        quint16* sizeField = (quint16*) (buffer+2);
        *sizeField = htons(outBufSize);
        m_RtpSession.sendBynaryResponse(buffer, outBufSize+4);
    }
    m_rtcpReportTimer.restart();
}

void QnMulticodecRtpReader::buildClientRTCPReport(quint8 chNumber)
{
    quint8 buffer[1024*4];
    buffer[0] = '$';
    buffer[1] = chNumber;
    int outBufSize = nx::streaming::rtp::buildClientRtcpReport(buffer+4, sizeof(buffer)-4);
    if (outBufSize > 0)
    {
        quint16* sizeField = (quint16*) (buffer+2);
        *sizeField = htons(outBufSize);
        m_RtpSession.sendBynaryResponse(buffer, outBufSize+4);
    }
}

void QnMulticodecRtpReader::processCameraTimeHelperEvent(
    nx::streaming::rtp::CameraTimeHelper::EventType event)
{
    using namespace nx::streaming::rtp;
    using namespace vms::api;
    auto currentTime = qnSyncTime->currentUSecsSinceEpoch();
    auto res = getResource();
    switch (event)
    {
        case CameraTimeHelper::EventType::BadCameraTime:
            emit networkIssue(res, currentTime, EventReason::networkBadCameraTime, "");
            return;
        case CameraTimeHelper::EventType::CameraTimeBackToNormal:
            emit networkIssue(res, currentTime, EventReason::networkCameraTimeBackToNormal, "");
            return;
        default:
            return;
    }
}

QnAbstractMediaDataPtr QnMulticodecRtpReader::getNextDataInternal()
{
    for (auto& track: m_tracks)
    {
        if (!track.parser)
            continue;
        QnAbstractMediaDataPtr result = track.parser->nextData();
        if (!result)
            continue;

        result->timestamp = m_timeHelper.getTime(
            qnSyncTime->currentTimePoint(),
            result->timestamp,
            track.ioDevice ? track.ioDevice->getSenderReport() : nx::streaming::rtp::RtcpSenderReport(),
            track.onvifExtensionTimestamp,
            track.parser->getFrequency(),
            m_role == Qn::CR_LiveVideo,
            [this](nx::streaming::rtp::CameraTimeHelper::EventType event) {
                processCameraTimeHelperEvent(event);
            }
        ).count();
        result->channelNumber = track.logicalChannelNum;
        if (result->dataType == QnAbstractMediaData::VIDEO)
        {
            result->channelNumber =
                std::min(result->channelNumber, (quint32) m_numberOfVideoChannels - 1);
        }
        return result;
    }
    return QnAbstractMediaDataPtr();
}

QnAbstractMediaDataPtr QnMulticodecRtpReader::getNextDataTCP()
{
    // int readed;
    int errorRetryCnt = 0;
    int rtpChannelNum = -1;

    m_dataTimer.restart();

    const auto tcpTimeout = m_RtpSession.getTCPTimeout();
    if (m_callbackTimeout.count() > 0)
        m_RtpSession.setTCPTimeout(m_callbackTimeout);
    const auto scopeGuard = nx::utils::makeScopeGuard([
        this, tcpTimeout]()
        {
            if (m_callbackTimeout.count() > 0)
                m_RtpSession.setTCPTimeout(tcpTimeout);
        });

    while (m_RtpSession.isOpened() && !m_pleaseStop && m_dataTimer.elapsed() <= m_rtpFrameTimeoutMs)
    {
        while (m_gotData)
        {
            QnAbstractMediaDataPtr data = getNextDataInternal();
            if (data)
                return data;
            else
                m_gotData = false;
        }

        m_RtpSession.sendKeepAliveIfNeeded(); //< RTSP GET_PARAMETER
        // RTCP keep-alive packet
        if (m_rtcpReportTimer.elapsed() >= RTCP_REPORT_TIMEOUT)
        {
            for (const TrackInfo& track: m_tracks)
            {
                if (track.ioDevice)
                    buildClientRTCPReport(track.rtcpChannelNumber);
            }
            m_rtcpReportTimer.restart();
        }

        int bytesRead = m_RtpSession.readBinaryResponce(m_demuxedData, rtpChannelNum);
        if (bytesRead < 0 &&
            (SystemError::getLastOSErrorCode() == SystemError::timedOut ||
             SystemError::getLastOSErrorCode() == SystemError::again) &&
            m_onSocketReadTimeoutCallback &&
            m_RtpSession.lastReceivedDataTimer().isValid() &&
            m_RtpSession.lastReceivedDataTimer().elapsed() < m_RtpSession.sessionTimeoutMs())
        {
            auto data = m_onSocketReadTimeoutCallback();
            if (data)
                return data;
            else
                continue;
        }

        auto trackIndexIter = m_trackIndices.find(rtpChannelNum);

        if (bytesRead < 1)
        {
            const bool isInvalidTrack = trackIndexIter == m_trackIndices.end();
            const bool isBufferAllocated = m_demuxedData.size() > rtpChannelNum
                && m_demuxedData[rtpChannelNum];

            if (isInvalidTrack && isBufferAllocated)
                m_demuxedData[rtpChannelNum]->clear();
            break; // error
        }

        if (trackIndexIter != m_trackIndices.end())
        {
            auto& track = m_tracks[trackIndexIter->second];
            int rtpBufferOffset = m_demuxedData[rtpChannelNum]->size() - bytesRead;
            const auto offset = rtpBufferOffset + kInterleavedRtpOverTcpPrefixLength;
            const auto length = bytesRead - kInterleavedRtpOverTcpPrefixLength;
            updateOnvifTime(offset, length, track, rtpChannelNum);
            if (!track.parser->processData(
                (quint8*)m_demuxedData[rtpChannelNum]->data(),
                offset,
                length,
                m_gotData))
            {
                clearKeyData(track.logicalChannelNum);
                m_demuxedData[rtpChannelNum]->clear();
                if (++errorRetryCnt > m_maxRtpRetryCount) {
                    qWarning() << "Too many RTP errors for camera " << getResource()->getName() << ". Reopen stream";
                    closeStream();
                    return QnAbstractMediaDataPtr(0);
                }
            }
            else if (m_gotData) {
                m_demuxedData[rtpChannelNum]->clear();
            }
        }
        else if (m_RtpSession.isRtcp(rtpChannelNum))
        {
            processTcpRtcp((quint8*) m_demuxedData[rtpChannelNum]->data(), bytesRead, m_demuxedData[rtpChannelNum]->capacity());
            m_demuxedData[rtpChannelNum]->clear();
        }
        else {
            m_demuxedData[rtpChannelNum]->clear();
        }
    }
    return  QnAbstractMediaDataPtr();
}

QnAbstractMediaDataPtr QnMulticodecRtpReader::getNextDataUDP()
{
    int errorRetryCount = 0;

    pollfd mediaSockPollArray[MAX_MEDIA_SOCKET_COUNT];
    memset( mediaSockPollArray, 0, sizeof(mediaSockPollArray) );

    m_dataTimer.restart();

    while (m_RtpSession.isOpened() && !m_pleaseStop && m_dataTimer.elapsed() <= m_rtpFrameTimeoutMs)
    {
        while (m_gotData)
        {
            QnAbstractMediaDataPtr data = getNextDataInternal();
            if (data)
                return data;
            else
                m_gotData = false;
        }

        int nfds = 0;
        std::vector<int> fdToRtpIndex(m_tracks.size());
        fdToRtpIndex.clear();

        for (int i = 0; i < m_tracks.size(); ++i)
        {
            const TrackInfo& track = m_tracks[i];
            if(track.ioDevice)
            {
                mediaSockPollArray[nfds].fd = track.ioDevice->getMediaSocket()->handle();
                mediaSockPollArray[nfds++].events = POLLIN;
                fdToRtpIndex.push_back(i);
            }
        }

        const int rez = poll( mediaSockPollArray, nfds, MEDIA_DATA_READ_TIMEOUT_MS );
        if (rez < 1)
            continue;
        for( int i = 0; i < nfds; ++i )
        {
            if (!(mediaSockPollArray[i].revents & POLLIN ))
                continue;

            const int rtpChannelNum = fdToRtpIndex[i];
            TrackInfo& track = m_tracks[rtpChannelNum];
            bool gotData = false;
            while (!gotData)
            {
                quint8* rtpBuffer = QnRtspClient::prepareDemuxedData(m_demuxedData, rtpChannelNum, MAX_RTP_PACKET_SIZE); // todo: update here
                int bytesRead = track.ioDevice->read( (char*) rtpBuffer, MAX_RTP_PACKET_SIZE);
                if (bytesRead < 1)
                    break;
                m_demuxedData[rtpChannelNum]->finishWriting(bytesRead);
                quint8* bufferBase = (quint8*) m_demuxedData[rtpChannelNum]->data();

                updateOnvifTime(rtpBuffer - bufferBase, bytesRead, track, rtpChannelNum);
                if (!track.parser->processData(
                        bufferBase,
                        rtpBuffer-bufferBase,
                        bytesRead,
                        gotData))
                {
                    clearKeyData(track.logicalChannelNum);
                    m_demuxedData[rtpChannelNum]->clear();
                    if (++errorRetryCount > m_maxRtpRetryCount) {
                        qWarning() << "Too many RTP errors for camera " << getResource()->getName() << ". Reopen stream";
                        closeStream();
                        return QnAbstractMediaDataPtr(0);
                    }
                }
            }
            if (gotData)
            {
                m_gotData = true;
                m_demuxedData[rtpChannelNum]->clear();
                break;
            }
        }
    }

    return QnAbstractMediaDataPtr();
}

nx::streaming::rtp::StreamParser* QnMulticodecRtpReader::createParser(const QString& codecName)
{
    nx::streaming::rtp::StreamParser* result = 0;
    if (codecName.isEmpty())
        return 0;
    else if (codecName == QLatin1String("H264"))
        result = new nx::streaming::rtp::H264Parser();
    else if (codecName == QLatin1String("H265"))
        result = new nx::streaming::rtp::HevcParser();
    else if (codecName == QLatin1String("JPEG"))
        result = new nx::streaming::rtp::MjpegParser;
    else if (codecName == QLatin1String("MPEG4-GENERIC"))
        result = new nx::streaming::rtp::AacParser;
    else if (codecName == QLatin1String("PCMU")) {
        nx::streaming::rtp::SimpleAudioParser* audioParser =
            new nx::streaming::rtp::SimpleAudioParser;
        audioParser->setCodecId(AV_CODEC_ID_PCM_MULAW);
        result = audioParser;
    }
    else if (codecName == QLatin1String("PCMA")) {
        nx::streaming::rtp::SimpleAudioParser* audioParser =
            new nx::streaming::rtp::SimpleAudioParser;
        audioParser->setCodecId(AV_CODEC_ID_PCM_ALAW);
        result = audioParser;
    }
    else if (codecName.startsWith(QLatin1String("G726"))) // g726-24, g726-32 e.t.c
    {
        int bitRatePos = codecName.indexOf(QLatin1Char('-'));
        if (bitRatePos == -1)
            return 0;
        QString bitsPerSample = codecName.mid(bitRatePos+1);
        nx::streaming::rtp::SimpleAudioParser* audioParser =
            new nx::streaming::rtp::SimpleAudioParser;
        audioParser->setCodecId(AV_CODEC_ID_ADPCM_G726);
        audioParser->setBitsPerSample(bitsPerSample.toInt()/8);
        audioParser->setSampleFormat(AV_SAMPLE_FMT_S16);
        result = audioParser;
    }
    else if (codecName == QLatin1String("L16")) {
        nx::streaming::rtp::SimpleAudioParser* audioParser
            = new nx::streaming::rtp::SimpleAudioParser;
        audioParser->setCodecId(AV_CODEC_ID_PCM_S16BE);
        audioParser->setSampleFormat(AV_SAMPLE_FMT_S16);
        result = audioParser;
    }

    if (result)
        Qn::directConnect(result, &nx::streaming::rtp::StreamParser::packetLostDetected, this, &QnMulticodecRtpReader::at_packetLost);

    return result;
}

void QnMulticodecRtpReader::at_propertyChanged(const QnResourcePtr & res, const QString & key)
{
    auto networkResource = res.dynamicCast<QnNetworkResource>();
    const bool isTransportChanged = key == QnMediaResource::rtpTransportKey()
        && getRtpTransport() != m_RtpSession.getTransport();
    const bool isMediaPortChanged = key == QnNetworkResource::mediaPortKey() && networkResource
        && networkResource->mediaPort() != m_RtpSession.getUrl()
            .port(nx::network::rtsp::DEFAULT_RTSP_PORT);
    if (isTransportChanged || isMediaPortChanged)
        pleaseStop();

    if (key == ResourcePropertyKey::kTrustCameraTime && m_role != Qn::ConnectionRole::CR_Archive)
        m_timeHelper.setTimePolicy(getTimePolicy(m_resource));
}

void QnMulticodecRtpReader::at_packetLost(quint32 prev, quint32 next)
{
    const QnResourcePtr& resource = getResource();
    NX_VERBOSE(this, "Packet loss detected on %1, prev seq %2, next seq %3", resource, prev, next);

    if (const auto camera = dynamic_cast<QnVirtualCameraResource*>(resource.data()))
        camera->issueOccured();

    const auto now = std::chrono::steady_clock::now();
    if (!m_packetLossReportTime || *m_packetLossReportTime + PACKET_LOSS_WARNING_TIME < now)
    {
        m_packetLossReportTime = now;
        NX_WARNING(this, "Packet loss detected on %1", resource);
    }

    emit networkIssue(
        resource, qnSyncTime->currentUSecsSinceEpoch(),
        vms::api::EventReason::networkRtpPacketLoss, QString());
}

nx::vms::api::RtpTransportType QnMulticodecRtpReader::getRtpTransport() const
{
    NX_MUTEX_LOCKER lock(&s_defaultTransportMutex);
    if (m_resource)
    {
        const auto rtpTransportString =
            m_resource->getProperty(QnMediaResource::rtpTransportKey());

        const auto transport = QnLexical::deserialized<nx::vms::api::RtpTransportType>(
            rtpTransportString,
            nx::vms::api::RtpTransportType::automatic);

        if (transport != nx::vms::api::RtpTransportType::automatic)
            return transport; //< User defined settings for resource.
        if (m_rtpTransport != nx::vms::api::RtpTransportType::automatic)
            return m_rtpTransport; //< Server side setting for resource.
    }

    return s_defaultTransportToUse;
}

void QnMulticodecRtpReader::setRtpTransport(nx::vms::api::RtpTransportType value )
{
    m_rtpTransport = value;
}

CameraDiagnostics::Result QnMulticodecRtpReader::openStream()
{
    m_pleaseStop = false;
    m_rtpStarted = false;
    if (isStreamOpened())
        return CameraDiagnostics::NoErrorResult();
    //m_timeHelper.reset();
    m_gotSomeFrame = false;
    m_RtpSession.setTransport(getRtpTransport());

    const QnNetworkResource* nres = dynamic_cast<QnNetworkResource*>(getResource().data());

    unregisterMulticastAddresses();
    calcStreamUrl();

    m_RtpSession.setAuth(nres->getAuth(), m_prefferedAuthScheme);

    m_tracks.clear();
    m_gotKeyDataInfo.clear();
    m_gotData = false;

    const qint64 position = m_positionUsec.exchange(AV_NOPTS_VALUE);
    m_openStreamResult = m_RtpSession.open(m_currentStreamUrl, position);
    if(m_openStreamResult.errorCode != CameraDiagnostics::ErrorCode::noError)
        return m_openStreamResult;

    QnVirtualCameraResourcePtr camera = qSharedPointerDynamicCast<QnVirtualCameraResource>(getResource());
    if (camera)
        m_RtpSession.setAudioEnabled(camera->isAudioEnabled());

    if (!m_RtpSession.playPhase1())
    {
        NX_WARNING(this, "Can't open RTSP stream [%1], SETUP request has been failed",
            m_currentStreamUrl);

        return CameraDiagnostics::RequestFailedResult(m_currentStreamUrl.toString(),
            "Can't open RTSP stream: SETUP request has been failed");
    }

    auto tracks = m_RtpSession.getTrackInfo();
    if (tracks.empty())
    {
        NX_WARNING(this, "Tracks are empty for [%1]", m_currentStreamUrl);
        return CameraDiagnostics::CameraInvalidParams("The list of tracks are empty");
    }

    std::set<QnRtspIoDevice::AddressInfo> addressInfoToRegister;
    for (const auto& track: tracks)
    {
        if (!track.ioDevice)
            continue;

        if (track.sdpMedia.mediaType != nx::streaming::Sdp::MediaType::Unknown)
        {
            addressInfoToRegister.insert(track.ioDevice->mediaAddressInfo());
            addressInfoToRegister.insert(track.ioDevice->rtcpAddressInfo());
        }
    }

    for (const auto& addressInfo: addressInfoToRegister)
    {
        if (!registerMulticastAddress(addressInfo))
        {
            return CameraDiagnostics::CameraInvalidParams(
                "Multicast media address conflict detected");
        }
    }

    if (!m_RtpSession.playPhase2(position, AV_NOPTS_VALUE, m_RtpSession.getScale()))
    {
        NX_WARNING(this, "Can't open RTSP stream [%1], PLAY request has been failed",
            m_currentStreamUrl);

        return CameraDiagnostics::RequestFailedResult(m_currentStreamUrl.toString(),
            "Can't open RTSP stream: PLAY request has been failed");
    }

    m_numberOfVideoChannels = camera && camera->allowRtspVideoLayout() ?
        m_RtpSession.getTrackCount(nx::streaming::Sdp::MediaType::Video) : 1;
    {
        QString manualConfiguredLayout = getConfiguredVideoLayout(m_resource);
        if (m_numberOfVideoChannels > 1 && manualConfiguredLayout.isEmpty())
        {
            QnMutexLocker lock( &m_layoutMutex );
            m_customVideoLayout.clear();
            QString newVideoLayout;
            m_customVideoLayout = QnCustomResourceVideoLayoutPtr(
                new QnCustomResourceVideoLayout(QSize(m_numberOfVideoChannels, 1)));
            for (int i = 0; i < m_numberOfVideoChannels; ++i)
                m_customVideoLayout->setChannel(i, 0, i); // arrange multi video layout from left to right

            newVideoLayout = m_customVideoLayout->toString();
            QnVirtualCameraResourcePtr camRes = m_resource.dynamicCast<QnVirtualCameraResource>();
            if (camRes && m_role == Qn::CR_LiveVideo &&
                camRes->setProperty(ResourcePropertyKey::kVideoLayout, newVideoLayout))
            {
                camRes->saveProperties();
            }
        }
    }

    createTrackParsers();

    bool videoExist = false;
    bool audioExist = false;
    for (const auto& track: m_RtpSession.getTrackInfo())
    {
        videoExist |= track.sdpMedia.mediaType == nx::streaming::Sdp::MediaType::Video;
        audioExist |= track.sdpMedia.mediaType == nx::streaming::Sdp::MediaType::Audio;
    }

    if (m_role == Qn::CR_LiveVideo)
    {
        if (audioExist) {
            if (!camera->isAudioSupported())
                camera->forceEnableAudio();
        }
        else {
            camera->forceDisableAudio();
        }
    }

    m_rtcpReportTimer.restart();
    if (!audioExist && !videoExist)
    {
        m_RtpSession.stop();
        m_openStreamResult = CameraDiagnostics::NoMediaTrackResult(m_currentStreamUrl.toString());
        return m_openStreamResult;
    }
    m_rtpStarted = true;
    m_openStreamResult = CameraDiagnostics::NoErrorResult();
    return CameraDiagnostics::NoErrorResult();
}

void QnMulticodecRtpReader::createTrackParsers()
{
    using namespace nx::streaming;
    auto& trackInfo = m_RtpSession.getTrackInfo();
    int logicalVideoNum = 0;
    bool forceRtcpReports = false;
    auto secResource = m_resource.dynamicCast<QnSecurityCamResource>();
    if (secResource)
    {
        auto resData = secResource->resourceData();
        forceRtcpReports = resData.value<bool>(lit("forceRtcpReports"), false);
    }
    for (auto& track: trackInfo)
    {
        bool supportedFormat =
            track.sdpMedia.mediaType == Sdp::MediaType::Video ||
            track.sdpMedia.mediaType == Sdp::MediaType::Audio;

        if (!track.setupSuccess || !track.ioDevice || !supportedFormat)
            continue;

        TrackInfo trackParser;
        trackParser.mediaType = track.sdpMedia.mediaType;
        trackParser.parser.reset(createParser(track.sdpMedia.rtpmap.codecName.toUpper()));
        if (!trackParser.parser)
        {
            NX_WARNING(this, "Failed to create track parser for codec: [%1]",
                track.sdpMedia.rtpmap.codecName);
            continue;
        }

        trackParser.parser->setSdpInfo(track.sdpMedia);
        trackParser.ioDevice = track.ioDevice.get();
        trackParser.rtcpChannelNumber = track.interleaved.second;
        trackParser.ioDevice->setForceRtcpReports(forceRtcpReports);

        rtp::AudioStreamParser* audioParser =
            dynamic_cast<rtp::AudioStreamParser*>(trackParser.parser.get());
        if (audioParser)
            m_audioLayout = audioParser->getAudioLayout();

        if (track.sdpMedia.mediaType == Sdp::MediaType::Video)
            trackParser.logicalChannelNum = logicalVideoNum++;
        else
            trackParser.logicalChannelNum = m_numberOfVideoChannels;

        if (m_RtpSession.getActualTransport() == nx::vms::api::RtpTransportType::tcp)
            m_trackIndices[track.interleaved.first] = m_tracks.size();

        m_tracks.push_back(trackParser);
    }
}

nx::network::rtsp::StatusCodeValue QnMulticodecRtpReader::getLastResponseCode() const
{
    return m_RtpSession.getLastResponseCode();
}

void QnMulticodecRtpReader::closeStream()
{
    m_rtpStarted = false;
    if (m_RtpSession.isOpened())
        m_RtpSession.sendTeardown();
    m_RtpSession.stop();
    for (unsigned int i = 0; i < m_demuxedData.size(); ++i) {
        if (m_demuxedData[i])
            m_demuxedData[i]->clear();
    }

    unregisterMulticastAddresses();
}

bool QnMulticodecRtpReader::isStreamOpened() const
{
    return m_RtpSession.isOpened();
}

CameraDiagnostics::Result QnMulticodecRtpReader::lastOpenStreamResult() const
{
    return m_openStreamResult;
}

QnConstResourceAudioLayoutPtr QnMulticodecRtpReader::getAudioLayout() const
{
    return m_audioLayout;
}

void QnMulticodecRtpReader::pleaseStop()
{
    m_pleaseStop = true;
    m_rtpStarted = false;
    m_RtpSession.shutdown();
}

void QnMulticodecRtpReader::setDefaultTransport(nx::vms::api::RtpTransportType rtpTransport)
{
    NX_INFO(typeid(QnMulticodecRtpReader), "Set default transport: %1", rtpTransport);

    NX_MUTEX_LOCKER lock(&s_defaultTransportMutex);
    s_defaultTransportToUse = rtpTransport;
}

void QnMulticodecRtpReader::setRole(Qn::ConnectionRole role)
{
    m_role = role;

    // Force camera time for NVR archives
    if (role == Qn::ConnectionRole::CR_Archive)
        m_timeHelper.setTimePolicy(nx::streaming::rtp::TimePolicy::forceCameraTime);
    else
        m_timeHelper.setTimePolicy(getTimePolicy(m_resource));
}

void QnMulticodecRtpReader::setPrefferedAuthScheme(const nx::network::http::header::AuthScheme::Value scheme){
    m_prefferedAuthScheme = scheme;
}

QnConstResourceVideoLayoutPtr QnMulticodecRtpReader::getVideoLayout() const
{
    QnMutexLocker lock( &m_layoutMutex );
    return m_customVideoLayout;
}

void QnMulticodecRtpReader::setUserAgent(const QString& value)
{
    m_RtpSession.setUserAgent(value);
}

nx::utils::Url QnMulticodecRtpReader::getCurrentStreamUrl() const
{
    return m_currentStreamUrl;
}

void QnMulticodecRtpReader::calcStreamUrl()
{
    auto nres = getResource().dynamicCast<QnNetworkResource>();
    if (!nres)
        return;

    int mediaPort = nres->mediaPort();
    if (m_request.startsWith(QLatin1String("rtsp://")))
    {
        m_currentStreamUrl = m_request;
        if (mediaPort)
            m_currentStreamUrl.setPort(mediaPort); //< Override port.
    }
    else
    {
        m_currentStreamUrl.clear();
        m_currentStreamUrl.setScheme("rtsp");
        m_currentStreamUrl.setHost(nres->getHostAddress());
        m_currentStreamUrl.setPort(mediaPort ? mediaPort : nx::network::rtsp::DEFAULT_RTSP_PORT);

        if (!m_request.isEmpty())
        {
            auto requestParts = m_request.split('?');
            QString path = requestParts[0];
            if (!path.startsWith(L'/'))
                path.insert(0, L'/');
            m_currentStreamUrl.setPath(path);
            if (requestParts.size() > 1)
                m_currentStreamUrl.setQuery(requestParts[1]);
        }
    }
}

void QnMulticodecRtpReader::setPositionUsec(qint64 value)
{
    m_positionUsec = value;
}

void QnMulticodecRtpReader::setDateTimeFormat(const QnRtspClient::DateTimeFormat& format)
{
    m_RtpSession.setDateTimeFormat(format);
}

void QnMulticodecRtpReader::addRequestHeader(
    const QString& requestName,
    const nx::network::http::HttpHeader& header)
{
    m_RtpSession.addRequestHeader(requestName, header);
}

QnRtspClient& QnMulticodecRtpReader::rtspClient()
{
    return m_RtpSession;
}

void QnMulticodecRtpReader::updateOnvifTime(
    int rtpBufferOffset,
    int rtpPacketSize,
    TrackInfo& track,
    int rtpChannel)
{
    uint8_t* data = (uint8_t*)m_demuxedData[rtpChannel]->data() + rtpBufferOffset;
    if (rtpPacketSize < nx::streaming::rtp::RtpHeader::kSize)
    {
        NX_WARNING(this, "RTP packet size is less than RTP header size, resetting onvif time");
        track.onvifExtensionTimestamp.reset();
        return;
    }

    const auto header = (nx::streaming::rtp::RtpHeader*) data;
    if (!header->extension)
        return;

    const int bytesTillExtension = header->payloadOffset();
    if (rtpPacketSize < bytesTillExtension)
    {
        NX_WARNING(this, "RTP packet size is less than expected, resetting onvif time");
        track.onvifExtensionTimestamp.reset();
        return;
    }

    data += bytesTillExtension;
    rtpPacketSize -= bytesTillExtension;
    nx::streaming::rtp::OnvifHeaderExtension onvifExtension;
    if (onvifExtension.read(data, rtpPacketSize))
        track.onvifExtensionTimestamp = onvifExtension.ntp;
}

CameraDiagnostics::Result QnMulticodecRtpReader::registerMulticastAddress(
    const QnRtspIoDevice::AddressInfo& addressInfo)
{
    if (addressInfo.transport != nx::vms::api::RtpTransportType::multicast)
        return CameraDiagnostics::NoErrorResult();

    auto resource = getResource();
    if (!NX_ASSERT(resource))
    {
        return CameraDiagnostics::InternalServerErrorResult(
            "Multicodec reader has no corresponding resource");
    }

    const auto& multicastAddress = addressInfo.address;
    if (!QHostAddress(multicastAddress.address.toString()).isMulticast()
        || multicastAddress.port <= 0)
    {
        emit networkIssue(
            resource,
            qnSyncTime->currentUSecsSinceEpoch(),
            vms::api::EventReason::networkMulticastAddressIsInvalid,
            QJson::serialized(multicastAddress));

        return CameraDiagnostics::CameraInvalidParams(
            "Address %1 is not a multicast address, though multicast transport has been chosen");
    }

    auto serverCamera = resource.dynamicCast<nx::vms::server::resource::Camera>();
    if (!NX_ASSERT(serverCamera,
            lm("Unable to convert resource %1 to nx::vms::server::resource::Camera").args(
            serverCamera)))
    {
        return CameraDiagnostics::InternalServerErrorResult(
            "Unable to convert resource to the needed type");
    }

    auto multicastAddressRegistry = serverCamera->serverModule()->multicastAddressRegistry();
    if (!NX_ASSERT(multicastAddressRegistry,
            lm("Unable to access multicast address registry for resource %1").args(serverCamera)))
    {
        return CameraDiagnostics::InternalServerErrorResult(
            "Unable to access multicast address registry");
    }

    if (!multicastAddressRegistry->registerAddress(serverCamera, multicastAddress))
    {
        const auto currentAddressUser = multicastAddressRegistry->addressUser(multicastAddress);
        const auto currentAddressUserName = currentAddressUser
            ? currentAddressUser->getUserDefinedName()
            : QString();

        nx::vms::event::NetworkIssueEvent::MulticastAddressConflictParameters
            eventParameters { multicastAddress, currentAddressUserName };

        emit networkIssue(
            serverCamera,
            qnSyncTime->currentUSecsSinceEpoch(),
            vms::api::EventReason::networkMulticastAddressConflict,
            QJson::serialized(eventParameters));

        return CameraDiagnostics::CameraInvalidParams(
            lm("Multicast address %1 is already in use by %2")
                .args(multicastAddress, currentAddressUserName));
    }
    else
    {
        m_registeredMulticastAddresses.insert(multicastAddress);
    }

    return CameraDiagnostics::NoErrorResult();
}

void QnMulticodecRtpReader::unregisterMulticastAddresses()
{
    auto serverCamera = getResource().dynamicCast<nx::vms::server::resource::Camera>();
    if (!serverCamera)
    {
        NX_WARNING(this,
            "%1() -> Unable to convert resource %2 to nx::vms::server::resource::Camera",
            __func__, m_resource);
        return;
    }

    auto multicastAddressRegistry = serverCamera->serverModule()->multicastAddressRegistry();
    if (!multicastAddressRegistry)
    {
        NX_WARNING(this,
            "%1() -> Unable to access multicast address registry for resource %2",
            __func__, m_resource);
        return;
    }

    for (const auto& multicastAddress: m_registeredMulticastAddresses)
        multicastAddressRegistry->unregisterAddress(multicastAddress);

    m_registeredMulticastAddresses.clear();
}

void QnMulticodecRtpReader::setOnSocketReadTimeoutCallback(
    std::chrono::milliseconds timeout,
    OnSocketReadTimeoutCallback callback)
{
    m_callbackTimeout = timeout;
    m_onSocketReadTimeoutCallback = std::move(callback);
}

void QnMulticodecRtpReader::setRtpFrameTimeoutMs(int value)
{
    m_rtpFrameTimeoutMs = value;
}

#endif // ENABLE_DATA_PROVIDERS
