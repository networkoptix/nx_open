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

#include <nx/streaming/rtp/parsers/i_rtp_parser_factory.h>

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

nx::Mutex QnMulticodecRtpReader::s_defaultTransportMutex;
nx::vms::api::RtpTransportType QnMulticodecRtpReader::s_defaultTransportToUse =
    nx::vms::api::RtpTransportType::automatic;

QnMulticodecRtpReader::QnMulticodecRtpReader(
    const QnResourcePtr& res,
    const nx::streaming::rtp::TimeOffsetPtr& timeOffset)
:
    QnResourceConsumer(res),
    m_RtpSession(QnRtspClient::Config()),
    m_timeHelper(res->getUniqueId().toStdString(), timeOffset),
    m_pleaseStop(false),
    m_gotSomeFrame(false),
    m_role(Qn::CR_Default),
    m_gotData(false),
    m_prefferedAuthScheme(nx::network::http::header::AuthScheme::digest),
    m_rtpTransport(nx::vms::api::RtpTransportType::automatic)
{
    const auto& globalSettings = res->commonModule()->globalSettings();
    m_logName = toString(res);
    m_rtpFrameTimeoutMs = globalSettings->rtpFrameTimeoutMs();
    m_maxRtpRetryCount = globalSettings->maxRtpRetryCount();

    m_RtpSession.setTCPTimeout(std::chrono::milliseconds(m_rtpFrameTimeoutMs));

    m_numberOfVideoChannels = 1;
    m_customVideoLayout.clear();

    QnSecurityCamResourcePtr camRes = qSharedPointerDynamicCast<QnSecurityCamResource>(res);
    if (camRes)
    {
        connect(this, &QnMulticodecRtpReader::networkIssue, camRes.data(),
            &QnSecurityCamResource::networkIssue, Qt::DirectConnection);

        m_ignoreRtcpReports = camRes->resourceData().value<bool>(
            ResourceDataKey::kIgnoreRtcpReports, m_ignoreRtcpReports);
    }
    Qn::directConnect(res.data(), &QnResource::propertyChanged,
        this, &QnMulticodecRtpReader::at_propertyChanged);

    m_timeHelper.setTimePolicy(getTimePolicy(m_resource));
}

QnMulticodecRtpReader::~QnMulticodecRtpReader()
{
    directDisconnectAll();

    for (unsigned int i = 0; i < m_demuxedData.size(); ++i)
        delete m_demuxedData[i];
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
    NX_VERBOSE(this, "Getting next data %1 (%2), role=%3", m_resource->getName(), m_resource->getId(), m_role);

    if (!isStreamOpened())
    {
        NX_VERBOSE(this, "Getting next data, stream is not opened, exiting, device: %1 (%2)",
            m_resource->getName(), m_resource->getId());

        return QnAbstractMediaDataPtr(0);
    }

    PlaybackRange playbackRange;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        std::swap(playbackRange, m_playbackRange);
    }

    if (playbackRange.isValid())
    {
        NX_VERBOSE(this, "Getting next data, seeking to position %1-%2 us, device %3 (%4)",
            playbackRange.startTimeUsec,
            playbackRange.endTimeUsec,
            m_resource->getName(), m_resource->getId());

        m_RtpSession.sendPlay(
            playbackRange.startTimeUsec,
            playbackRange.endTimeUsec,
            m_RtpSession.getScale());

        createTrackParsers();
    }

    QnAbstractMediaDataPtr result;
    do
    {
        m_dataTimer.restart();
        if (m_RtpSession.getActualTransport() == nx::vms::api::RtpTransportType::tcp)
            result = getNextDataTCP();
        else
            result = getNextDataUDP();

    } while(result && !gotKeyData(result));

    if (result)
    {
        NX_VERBOSE(this, "Got some stream data, device %1 (%2) dataType(%3), role=%4",
            m_resource->getName(), m_resource->getId(), result->dataType,
            m_role);

        m_gotSomeFrame = true;
        return result;
    }

    if (m_pleaseStop)
    {
        NX_VERBOSE(this, "Device %1 (%2). RTSP terminated because of pleaseStop call",
            m_resource->getName(), m_resource->getId());
        return result;
    }

    if (!m_gotSomeFrame)
    {
        NX_VERBOSE(this, "No frame has arrived since the stream was opened, device %1 (%2), role=%3",
            m_resource->getName(), m_resource->getId(), m_role);
        return result; // if no frame received yet do not report network issue error
    }

    int elapsed = m_dataTimer.elapsed();
    QString reasonParamsEncoded;
    vms::api::EventReason reason;

    if (m_RtpSession.isOpened())
    {
        if (!m_lastRtpParseResult.success)
        {
            reason = vms::api::EventReason::networkRtpParserError;
            reasonParamsEncoded = nx::vms::event::NetworkIssueEvent::encodePrimaryStream(
                m_role != Qn::CR_SecondaryLiveVideo, m_lastRtpParseResult.errorMessage);
        }
        else
        {
            reason = vms::api::EventReason::networkNoFrame;
            reasonParamsEncoded = vms::event::NetworkIssueEvent::encodeTimeoutMsecs(elapsed);
            NX_WARNING(this, "Can not read RTP frame for camera %1 during %2 ms, m_role=%3",
                getResource(), elapsed, m_role);
        }
    }
    else
    {
        reason = vms::api::EventReason::networkConnectionClosed;
        reasonParamsEncoded = vms::event::NetworkIssueEvent::encodePrimaryStream(m_role != Qn::CR_SecondaryLiveVideo);
        NX_WARNING(this, "%1: RTP connection was forcibly closed by camera. Reopen stream",
            m_logName);
    }

    emit networkIssue(getResource(),
        qnSyncTime->currentUSecsSinceEpoch(),
        reason,
        reasonParamsEncoded);
    QnVirtualCameraResource* cam = dynamic_cast<QnVirtualCameraResource*>(getResource().data());
    if (cam)
        cam->issueOccured();

    NX_VERBOSE(this, "No video frame for camera %1", m_resource->getUrl());
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

        nx::streaming::rtp::RtcpSenderReport rtcpReport;
        if (!m_ignoreRtcpReports && track.ioDevice)
            rtcpReport = track.ioDevice->getSenderReport();

        result->timestamp = m_timeHelper.getTime(
            qnSyncTime->currentTimePoint(),
            result->timestamp,
            rtcpReport,
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

        int bytesRead = m_RtpSession.readBinaryResponse(m_demuxedData, rtpChannelNum);
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
            const bool isBufferAllocated = (int) m_demuxedData.size() > rtpChannelNum
                && rtpChannelNum >= 0
                && m_demuxedData[rtpChannelNum];

            if (isInvalidTrack && isBufferAllocated)
                m_demuxedData[rtpChannelNum]->clear();
            break; // error
        }

        if (trackIndexIter != m_trackIndices.end())
        {
            auto& track = m_tracks[trackIndexIter->second];
            const int rtpBufferOffset = m_demuxedData[rtpChannelNum]->size() - bytesRead;
            const int offset = rtpBufferOffset + kInterleavedRtpOverTcpPrefixLength;
            const int length = bytesRead - kInterleavedRtpOverTcpPrefixLength;
            uint8_t* rtpPacketData = (uint8_t*)m_demuxedData[rtpChannelNum]->data();
            updateOnvifTime(rtpPacketData + offset, length, track);
            m_lastRtpParseResult = 
                track.parser->processData(rtpPacketData, offset, length, m_gotData);
            if (!m_lastRtpParseResult.success)
            {
                NX_DEBUG(this, "%1: %2", m_logName, m_lastRtpParseResult.errorMessage);
                clearKeyData(track.logicalChannelNum);
                m_demuxedData[rtpChannelNum]->clear();
                if (++errorRetryCnt > m_maxRtpRetryCount)
                {
                    NX_WARNING(this, "%1: Too many RTP errors. Report a network issue", m_logName);
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
                {
                    auto errorCode = SystemError::getLastOSErrorCode();
                    if (errorCode != SystemError::timedOut && errorCode != SystemError::again)
                    {
                        NX_DEBUG(this, "%1: Failed to read from UDP socket, OS code %2: %3",
                            m_logName, errorCode, SystemError::toString(errorCode));
                    }
                    break;
                }
                NX_VERBOSE(this, "%1: %2 bytes read form UDP socket", m_logName, bytesRead);
                m_demuxedData[rtpChannelNum]->finishWriting(bytesRead);
                quint8* bufferBase = (quint8*) m_demuxedData[rtpChannelNum]->data();

                updateOnvifTime(rtpBuffer, bytesRead, track);
                m_lastRtpParseResult = track.parser->processData(
                    bufferBase,
                    rtpBuffer - bufferBase,
                    bytesRead,
                    gotData);
                if (!m_lastRtpParseResult.success)
                {
                    NX_DEBUG(this, "%1: %2", m_logName, m_lastRtpParseResult.errorMessage);
                    clearKeyData(track.logicalChannelNum);
                    m_demuxedData[rtpChannelNum]->clear();
                    if (++errorRetryCount > m_maxRtpRetryCount) 
                    {
                        NX_WARNING(this, "%1: Too many RTP errors. Report a network issue", m_logName);
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
    {
        auto parser = new nx::streaming::rtp::MjpegParser;

        // Forward configured resolution, that can be used when resolution more than 2048, and
        // camera not send any adittional info.
        auto serverCamera = getResource().dynamicCast<nx::vms::server::resource::Camera>();
        if (serverCamera)
        {
            auto params = serverCamera->advancedLiveStreamParams();
            QSize& resolution = (m_role == Qn::CR_LiveVideo ? params.primaryStream.resolution :
                params.secondaryStream.resolution);
            parser->setConfiguredResolution(resolution.width(), resolution.height());
        }
        result = parser;
    }
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
    else if (m_customTrackParserFactory)
    {
        std::unique_ptr<nx::streaming::rtp::StreamParser> customParser =
            m_customTrackParserFactory->createParser(codecName);

        if (customParser)
            result = customParser.release();
    }

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
    {
        NX_DEBUG(this, "%1: Transport type or media port changed, stopping. Transport type: %2",
            m_logName, getRtpTransport());
        pleaseStop();
    }

    if (key == ResourcePropertyKey::kTrustCameraTime && m_role != Qn::ConnectionRole::CR_Archive)
        m_timeHelper.setTimePolicy(getTimePolicy(m_resource));
}

nx::vms::api::RtpTransportType QnMulticodecRtpReader::getRtpTransport() const
{
    // Client defined settings for resource.
    if (m_resource)
    {
        const auto rtpTransportString =
            m_resource->getProperty(QnMediaResource::rtpTransportKey());

        const auto transport = QnLexical::deserialized<nx::vms::api::RtpTransportType>(
            rtpTransportString,
            nx::vms::api::RtpTransportType::automatic);

        if (transport != nx::vms::api::RtpTransportType::automatic)
            return transport;
    }

    // Server side setting for resource.
    if (m_rtpTransport != nx::vms::api::RtpTransportType::automatic)
        return m_rtpTransport;

    NX_MUTEX_LOCKER lock(&s_defaultTransportMutex);
    return s_defaultTransportToUse;
}

void QnMulticodecRtpReader::setRtpTransport(nx::vms::api::RtpTransportType value)
{
    m_rtpTransport = value;
}

void QnMulticodecRtpReader::setCustomTrackParserFactory(
    std::unique_ptr<nx::streaming::rtp::IRtpParserFactory> parserFactory)
{
    m_customTrackParserFactory = std::move(parserFactory);
}

CameraDiagnostics::Result QnMulticodecRtpReader::openStream()
{
    m_pleaseStop = false;
    if (isStreamOpened())
        return CameraDiagnostics::NoErrorResult();
    //m_timeHelper.reset();
    m_gotSomeFrame = false;
    m_RtpSession.setTransport(getRtpTransport());

    const QnNetworkResource* nres = dynamic_cast<QnNetworkResource*>(getResource().data());

    m_registeredMulticastAddresses.clear();
    calcStreamUrl();

    m_RtpSession.setAuth(nres->getAuth(), m_prefferedAuthScheme);

    m_tracks.clear();
    m_trackIndices.clear();
    m_gotKeyDataInfo.clear();
    m_gotData = false;

    PlaybackRange playbackRange;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        std::swap(playbackRange, m_playbackRange);
    }

    m_openStreamResult = m_RtpSession.open(m_currentStreamUrl, playbackRange.startTimeUsec);
    if(m_openStreamResult.errorCode != CameraDiagnostics::ErrorCode::noError)
        return m_openStreamResult;

    if (m_customTrackParserFactory)
    {
        const auto tracks = m_RtpSession.getTrackInfo();
        std::set<QString> additionalSupportedCodecs;
        for (const QnRtspClient::SDPTrackInfo& track: tracks)
        {
            const QString& codecName = track.sdpMedia.rtpmap.codecName;
            if (m_customTrackParserFactory->supportsCodec(codecName))
                additionalSupportedCodecs.insert(codecName);
        }

        m_RtpSession.setAdditionalSupportedCodecs(std::move(additionalSupportedCodecs));
    }

    QnVirtualCameraResourcePtr camera = qSharedPointerDynamicCast<QnVirtualCameraResource>(getResource());
    if (camera)
        m_RtpSession.setAudioEnabled(camera->isAudioEnabled());

    if (!m_RtpSession.sendSetup())
    {
        NX_WARNING(this, "Can't open RTSP stream [%1], SETUP request has been failed",
            m_currentStreamUrl);

        return CameraDiagnostics::RequestFailedResult(m_currentStreamUrl.toString(),
            "Can't open RTSP stream: SETUP request has been failed");
    }

    if (const auto result = registerMulticastAddressesIfNeeded(); !result)
        return result;

    if (!m_RtpSession.sendPlay(
        playbackRange.startTimeUsec,
        playbackRange.endTimeUsec,
        m_RtpSession.getScale()))
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
        m_openStreamResult = CameraDiagnostics::NoMediaTrackResult(m_currentStreamUrl);
        return m_openStreamResult;
    }
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
        if (!track.setupSuccess || !track.ioDevice || !isFormatSupported(track.sdpMedia))
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

void QnMulticodecRtpReader::closeStream()
{
    if (m_RtpSession.isOpened())
        m_RtpSession.sendTeardown();
    m_RtpSession.stop();
    for (unsigned int i = 0; i < m_demuxedData.size(); ++i) {
        if (m_demuxedData[i])
            m_demuxedData[i]->clear();
    }

    m_registeredMulticastAddresses.clear();
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
    m_logName += "(Role: " + toString(role) + ")";

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
    if (m_request.startsWith("rtsp://") || m_request.startsWith("rtsps://"))
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

void QnMulticodecRtpReader::setPlaybackRange(int64_t startTimeUsec, int64_t endTimeUsec)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_playbackRange = PlaybackRange(startTimeUsec, endTimeUsec);
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

void QnMulticodecRtpReader::updateOnvifTime(const uint8_t* data, int size, TrackInfo& track)
{
    if (size < nx::streaming::rtp::RtpHeader::kSize)
    {
        NX_WARNING(this, "RTP packet size is less than RTP header size, resetting onvif time");
        track.onvifExtensionTimestamp.reset();
        return;
    }

    const auto header = (nx::streaming::rtp::RtpHeader*) data;
    if (!header->extension)
        return;

    const int bytesTillExtension = header->payloadOffset();
    if (size < bytesTillExtension)
    {
        NX_WARNING(this, "RTP packet size is less than expected, resetting onvif time");
        track.onvifExtensionTimestamp.reset();
        return;
    }

    data += bytesTillExtension;
    size -= bytesTillExtension;
    nx::streaming::rtp::OnvifHeaderExtension onvifExtension;
    if (onvifExtension.read(data, size))
        track.onvifExtensionTimestamp = onvifExtension.ntp;
}

CameraDiagnostics::Result QnMulticodecRtpReader::registerMulticastAddressesIfNeeded()
{
    auto tracks = m_RtpSession.getTrackInfo();
    if (tracks.empty())
    {
        NX_WARNING(this, "%1: Tracks are empty for [%2]", m_logName, m_currentStreamUrl);
        return CameraDiagnostics::CameraInvalidParams("The list of tracks are empty");
    }

    std::set<QnRtspIoDevice::AddressInfo> addressInfoToRegister;
    for (const auto& track: tracks)
    {
        if (!track.ioDevice || !track.setupSuccess)
            continue;

        addressInfoToRegister.insert(track.ioDevice->mediaAddressInfo());
        addressInfoToRegister.insert(track.ioDevice->rtcpAddressInfo());
    }

    for (const auto& addressInfo: addressInfoToRegister)
    {
        if (!registerAddressIfNeeded(addressInfo))
        {
            return CameraDiagnostics::CameraInvalidParams(
                "Multicast media address conflict detected");
        }
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnMulticodecRtpReader::registerAddressIfNeeded(
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
        lm("Unable to convert resource %1 to nx::vms::server::resource::Camera")
            .args(serverCamera)))
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

    auto registeredAddress = multicastAddressRegistry->registerAddress(
        serverCamera,
        Qn::toStreamIndex(m_role),
        multicastAddress);

    if (registeredAddress)
    {
        m_registeredMulticastAddresses.insert(std::move(registeredAddress));
    }
    else
    {
        const auto addressUsageInfo =
            multicastAddressRegistry->addressUsageInfo(multicastAddress);

        const auto device = addressUsageInfo.device.toStrongRef();
        const auto deviceName = device ? device->getUserDefinedName() : QString();

        nx::vms::event::NetworkIssueEvent::MulticastAddressConflictParameters
            eventParameters{ multicastAddress, deviceName, addressUsageInfo.stream };

        emit networkIssue(
            serverCamera,
            qnSyncTime->currentUSecsSinceEpoch(),
            vms::api::EventReason::networkMulticastAddressConflict,
            QJson::serialized(eventParameters));

        return CameraDiagnostics::CameraInvalidParams(
            lm("Multicast address %1 is already in use by %2")
                .args(multicastAddress, deviceName));
    }

    return CameraDiagnostics::NoErrorResult();
}

bool QnMulticodecRtpReader::isFormatSupported(const nx::streaming::Sdp::Media media) const
{
    bool supportedByCustomParser = false;
    if (m_customTrackParserFactory)
    {
        supportedByCustomParser =
            m_customTrackParserFactory->supportsCodec(media.rtpmap.codecName);
    }

    return media.mediaType == nx::streaming::Sdp::MediaType::Audio
        || media.mediaType == nx::streaming::Sdp::MediaType::Video
        || supportedByCustomParser;
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

