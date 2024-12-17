// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rtsp_stream_provider.h"

#include <set>

#include <QtCore/QSettings>

#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/resource_media_layout.h>
#include <core/resource_management/resource_data_pool.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/network/compat_poll.h>
#include <nx/network/rtsp/rtsp_types.h>
#include <nx/reflect/string_conversion.h>
#include <nx/rtp/parsers/aac_rtp_parser.h>
#include <nx/rtp/parsers/h264_rtp_parser.h>
#include <nx/rtp/parsers/hevc_rtp_parser.h>
#include <nx/rtp/parsers/i_rtp_parser_factory.h>
#include <nx/rtp/parsers/mjpeg_rtp_parser.h>
#include <nx/rtp/parsers/mpeg12_audio_rtp_parser.h>
#include <nx/rtp/parsers/mpeg4_rtp_parser.h>
#include <nx/rtp/parsers/rtp_parser.h>
#include <nx/rtp/parsers/simpleaudio_rtp_parser.h>
#include <nx/rtp/rtp.h>
#include <nx/streaming/rtp/parsers/nx_rtp_metadata_parser.h>
#include <nx/streaming/rtp/parsers/nx_rtp_parser.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/api/types/rtp_types.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/rules/network_issue_info.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>

using namespace nx;

namespace {

static const int RTCP_REPORT_TIMEOUT = 30 * 1000;

static const int MAX_MEDIA_SOCKET_COUNT = 5;
static const int MEDIA_DATA_READ_TIMEOUT_MS = 100;

// prefix has the following format $<ChannelId(1byte)><PayloadLength(2bytes)>
static const int kInterleavedRtpOverTcpPrefixLength = 4;

QString getConfiguredVideoLayout(const QnVirtualCameraResourcePtr& device)
{
    if (auto layout = device->resourceData().value<QString>(ResourceDataKey::kVideoLayout);
        !layout.isEmpty())
    {
        return layout;
    }

    if (QnResourceTypePtr resType = qnResTypePool->getResourceType(device->getTypeId()))
        return resType->defaultValue(ResourcePropertyKey::kVideoLayout);

    return {};
}

bool isEmptyMediaAllowed(const QnResourcePtr& res)
{
    if (const auto camera = res.dynamicCast<QnVirtualCameraResource>())
    {
        using namespace nx::vms::api;
        const auto deviceType = camera->deviceType();
        return deviceType == DeviceType::ioModule || deviceType == DeviceType::hornSpeaker;
    }
    return false;
}

} // namespace

namespace nx::streaming {

nx::vms::api::RtpTransportType RtspStreamProvider::s_defaultTransportToUse =
    nx::vms::api::RtpTransportType::automatic;

RtspStreamProvider::RtspStreamProvider(
    const nx::vms::common::SystemSettings* systemSetting,
    const nx::streaming::rtp::TimeOffsetPtr& timeOffset)
:
    m_RtpSession(QnRtspClient::Config{}),
    m_timeOffset(timeOffset),
    m_pleaseStop(false),
    m_gotSomeFrame(false),
    m_role(Qn::CR_Default),
    m_gotData(false),
    m_preferredAuthScheme(nx::network::http::header::AuthScheme::digest),
    m_rtpTransport(nx::vms::api::RtpTransportType::automatic)
{
    m_rtpFrameTimeoutMs = systemSetting->rtpFrameTimeoutMs();
    m_maxRtpRetryCount = systemSetting->maxRtpRetryCount();

    m_RtpSession.setTCPTimeout(std::chrono::milliseconds(m_rtpFrameTimeoutMs));

    m_numberOfVideoChannels = 1;
    m_customVideoLayout.reset();
}

RtspStreamProvider::~RtspStreamProvider()
{
    for (unsigned int i = 0; i < m_demuxedData.size(); ++i)
        delete m_demuxedData[i];
}

nx::network::MulticastAddressRegistry* RtspStreamProvider::multicastAddressRegistry()
{
    return nullptr;
}

QnAdvancedStreamParams RtspStreamProvider::advancedLiveStreamParams() const
{
    return QnAdvancedStreamParams{};
}

void RtspStreamProvider::reportNetworkIssue(
        std::chrono::microseconds /*timestamp*/,
        nx::vms::api::EventReason /*reasonCode*/,
        const nx::vms::rules::NetworkIssueInfo& /*info*/)
{
}

void RtspStreamProvider::setRequest(const QString& request)
{
    m_request = request;
    updateStreamUrlIfNeeded();
}

void RtspStreamProvider::clearKeyData(int channelNum)
{
    if (m_gotKeyDataInfo.size() > (size_t)channelNum)
        m_gotKeyDataInfo[channelNum] = false;
}

bool RtspStreamProvider::gotKeyData(const QnAbstractMediaDataPtr& mediaData)
{
    if ((size_t)m_gotKeyDataInfo.size() <= mediaData->channelNumber)
    {
        size_t oldSize = m_gotKeyDataInfo.size();
        m_gotKeyDataInfo.resize(mediaData->channelNumber + 1);
        for (size_t i = oldSize; i < m_gotKeyDataInfo.size(); ++i)
            m_gotKeyDataInfo[i] = false;
    }

    if (mediaData->dataType == QnAbstractMediaData::VIDEO)
    {
        if (mediaData->flags & AV_PKT_FLAG_KEY)
            m_gotKeyDataInfo[mediaData->channelNumber] = true;
        return m_gotKeyDataInfo[mediaData->channelNumber];
    }
    return true;
}

QnAbstractMediaDataPtr RtspStreamProvider::getNextData()
{
    NX_VERBOSE(this, "%1: Getting next data", m_logName);
    if (!isStreamOpened())
    {
        NX_VERBOSE(this, "%1: Getting next data, stream is not opened, exiting", m_logName);
        return QnAbstractMediaDataPtr(0);
    }

    PlaybackRange playbackRange;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        std::swap(playbackRange, m_playbackRange);
    }

    if (playbackRange.isValid())
    {
        NX_VERBOSE(this, "%1: Getting next data, seeking to position %2-%3 us",
            m_logName,
            playbackRange.startTimeUsec,
            playbackRange.endTimeUsec);

        m_RtpSession.sendPlay(
            playbackRange.startTimeUsec,
            playbackRange.endTimeUsec,
            m_RtpSession.getScale());

        createTrackParsers();
    }

    QnAbstractMediaDataPtr result;

    m_tcpSocket = m_RtpSession.tcpSock();
    if (!m_tcpSocket)
    {
        NX_DEBUG(this, "%1: Getting next data, stream is not opened, socket is empty", m_logName);
        return nullptr;
    }

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
        NX_VERBOSE(this, "%1: Got some stream data, dataType: %2", m_logName, result->dataType);

        m_gotSomeFrame = true;
        return result;
    }

    if (m_pleaseStop)
    {
        NX_VERBOSE(this, "%1: RTSP terminated because of pleaseStop call", m_logName);
        return result;
    }

    if (!m_gotSomeFrame)
    {
        NX_VERBOSE(this, "%1: No frame has arrived since the stream was opened", m_logName);
        return result; // if no frame received yet do not report network issue error
    }

    const auto elapsed = std::chrono::milliseconds(m_dataTimer.elapsed());
    nx::vms::rules::NetworkIssueInfo info;
    vms::api::EventReason reason;

    if (m_RtpSession.isOpened())
    {
        if (!m_lastRtpParseResult.success)
        {
            reason = vms::api::EventReason::networkRtpStreamError;
            info.stream = (m_role == Qn::CR_SecondaryLiveVideo)
                ? nx::vms::api::StreamIndex::secondary
                : nx::vms::api::StreamIndex::primary;
            info.message = m_lastRtpParseResult.message;
        }
        else
        {
            reason = vms::api::EventReason::networkNoFrame;
            info.timeout = elapsed;
            NX_WARNING(this, "Can not read RTP frame for camera %1 during %2 ms, m_role=%3",
                m_logName, elapsed, m_role);
        }
    }
    else
    {
        reason = vms::api::EventReason::networkConnectionClosed;
        info.stream = (m_role == Qn::CR_SecondaryLiveVideo)
            ? nx::vms::api::StreamIndex::secondary
            : nx::vms::api::StreamIndex::primary;

        NX_WARNING(this, "%1: RTP connection was forcibly closed by camera. Reopen stream",
            m_logName);
    }

    reportNetworkIssue(qnSyncTime->currentTimePoint(), reason, info);

    NX_VERBOSE(this, "No video frame for URL %1", m_url);
    return result;
}

void RtspStreamProvider::processTcpRtcp(quint8* buffer, int bufferSize, int bufferCapacity)
{
    if (!m_RtpSession.processTcpRtcpData(buffer, bufferSize))
        NX_VERBOSE(this, "Can't parse RTCP report");
    int outBufSize = nx::rtp::buildClientRtcpReport(buffer+4, bufferCapacity-4);
    if (outBufSize > 0)
    {
        quint16* sizeField = (quint16*) (buffer+2);
        *sizeField = htons(outBufSize);
        m_RtpSession.sendBynaryResponse(buffer, outBufSize+4);
    }
    m_rtcpReportTimer.restart();
}

void RtspStreamProvider::buildClientRTCPReport(quint8 chNumber)
{
    quint8 buffer[1024*4];
    buffer[0] = '$';
    buffer[1] = chNumber;
    int outBufSize = nx::rtp::buildClientRtcpReport(buffer+4, sizeof(buffer)-4);
    if (outBufSize > 0)
    {
        quint16* sizeField = (quint16*) (buffer+2);
        *sizeField = htons(outBufSize);
        m_RtpSession.sendBynaryResponse(buffer, outBufSize+4);
    }
}

void RtspStreamProvider::processCameraTimeHelperEvent(
    nx::streaming::rtp::CameraTimeHelper::EventType event)
{
    using namespace nx::streaming::rtp;
    using namespace vms::api;
    const auto currentTime = qnSyncTime->currentTimePoint();
    switch (event)
    {
        case CameraTimeHelper::EventType::BadCameraTime:
            reportNetworkIssue(currentTime, EventReason::networkBadCameraTime, {});
            return;
        case CameraTimeHelper::EventType::CameraTimeBackToNormal:
            reportNetworkIssue(currentTime, EventReason::networkCameraTimeBackToNormal, {});
            return;
        default:
            return;
    }
}

QnAbstractMediaDataPtr RtspStreamProvider::getNextDataInternal()
{
    for (auto& track: m_tracks)
    {
        for (const auto& [payloadType, parser]: track.rtpParsers)
        {
            if (!parser)
                continue;

            nx::rtp::RtcpSenderReport rtcpReport;
            if (!m_ignoreRtcpReports && track.ioDevice)
                rtcpReport = track.ioDevice->getSenderReport();

            QnAbstractMediaDataPtr result = parser->nextData(rtcpReport);
            if (!result)
                continue;
            if (!m_forceCameraTime)
            {
                result->timestamp = track.timeHelper->getTime(
                    qnSyncTime->currentTimePoint(),
                    std::chrono::microseconds(result->timestamp),
                    parser->isUtcTime(),
                    m_role == Qn::CR_LiveVideo && track.logicalChannelNum == 0,
                    [this](nx::streaming::rtp::CameraTimeHelper::EventType event)
                    {
                        processCameraTimeHelperEvent(event);
                    }
                ).count();
            }
            result->channelNumber = track.logicalChannelNum;
            if (result->dataType == QnAbstractMediaData::VIDEO)
            {
                result->channelNumber =
                    std::min(result->channelNumber, (quint32) m_numberOfVideoChannels - 1);
            }
            else if (result->dataType == QnAbstractMediaData::AUDIO && !m_audioLayout)
            {
                m_audioLayout.reset(new AudioLayout(result->context));
            }
            return result;
        }
    }
    return QnAbstractMediaDataPtr();
}

bool RtspStreamProvider::processData(
    TrackInfo& track, uint8_t* buffer, int offset, int size, int channel, int& errorCount)
{
    bool packetLoss = false;

    const auto payloadType = nx::rtp::RtpHeader::getPayloadType(buffer + offset, size);
    if (!payloadType)
    {
        NX_DEBUG(this, "%1 Invalid RTP header: Unable to extract payload type", m_logName);
        return false;
    }

    const auto it = track.rtpParsers.find(*payloadType);
    if (it == track.rtpParsers.end())
    {
        NX_VERBOSE(this, "%1 Unable to find parser for payload type %2", m_logName, *payloadType);
        return true;
    }

    const auto& parser = it->second;
    auto result = parser->processData(buffer, offset, size, packetLoss, m_gotData);

    if (!result.success)
    {
        NX_DEBUG(this, "%1 RTP parsing error: %2", m_logName, result.message);
        clearKeyData(track.logicalChannelNum);
        m_demuxedData[channel]->clear();
        parser->clear();
    }

    if (!result.success || packetLoss)
    {
        if (++errorCount > m_maxRtpRetryCount)
        {
            m_lastRtpParseResult = result;
            if (result.success && packetLoss)
                m_lastRtpParseResult = {false, "RTP packet loss"};
            NX_WARNING(this, "%1: Too many RTP errors. Report a network issue", m_logName);
            return false;
        }
    }

    if (m_gotData)
        m_demuxedData[channel]->clear();

    return true;
}

bool RtspStreamProvider::isCodecSupportedByCustomParserFactories(const QString& codecName) const
{
    return std::any_of(
        m_customParserFactories.begin(),
        m_customParserFactories.end(),
        [&codecName](const auto& factory) { return factory->supportsCodec(codecName); });
}

bool RtspStreamProvider::isDataTimerExpired() const
{
    return m_rtpFrameTimeoutMs > 0 && m_dataTimer.elapsed() > m_rtpFrameTimeoutMs;
}

QnAbstractMediaDataPtr RtspStreamProvider::getNextDataTCP()
{
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

    while (m_tcpSocket->isConnected() && !m_pleaseStop && !isDataTimerExpired())
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
        if (bytesRead < 0 && !nx::network::socketCannotRecoverFromError(SystemError::getLastOSErrorCode()))
        {
            bool isTimeout = SystemError::getLastOSErrorCode() == SystemError::timedOut ||
                SystemError::getLastOSErrorCode() == SystemError::again;
            if (isTimeout
                && m_onSocketReadTimeoutCallback
                && m_RtpSession.lastReceivedDataTimer().isValid()
                && m_RtpSession.lastReceivedDataTimer().elapsed() < m_RtpSession.sessionTimeoutMs())
            {
                if (auto data = m_onSocketReadTimeoutCallback())
                    return data;
            }
            continue;
        }

        auto trackIndexIter = m_trackIndices.find(rtpChannelNum);
        if (trackIndexIter == m_trackIndices.end() && m_RtpSession.isPlayNowMode())
        {
            registerPredefinedTrack(rtpChannelNum);
            trackIndexIter = m_trackIndices.find(rtpChannelNum);
        }

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
        NX_VERBOSE(this, "Got %1 bytes, trackFound=%2", bytesRead, trackIndexIter != m_trackIndices.end());

        if (trackIndexIter != m_trackIndices.end())
        {
            auto& track = m_tracks[trackIndexIter->second];

            if (m_demuxedData[rtpChannelNum]->size() > MAX_ALLOWED_FRAME_SIZE)
            {
                for (auto& [_, parser]: track.rtpParsers)
                    parser->clear();
                clearKeyData(track.logicalChannelNum);
                m_demuxedData[rtpChannelNum]->clear();
                NX_WARNING(this, "Cleanup data for RTP track %1, URL %2. Buffer overflow", rtpChannelNum, m_url);
                continue;
            }

            const int rtpBufferOffset = m_demuxedData[rtpChannelNum]->size() - bytesRead;
            const int offset = rtpBufferOffset + kInterleavedRtpOverTcpPrefixLength;
            const int length = bytesRead - kInterleavedRtpOverTcpPrefixLength;
            uint8_t* rtpPacketData = (uint8_t*)m_demuxedData[rtpChannelNum]->data();
            if (!processData(track, rtpPacketData, offset, length, rtpChannelNum, errorRetryCnt))
                return QnAbstractMediaDataPtr(0);
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

QnAbstractMediaDataPtr RtspStreamProvider::getNextDataUDP()
{
    int errorRetryCount = 0;

    pollfd mediaSockPollArray[MAX_MEDIA_SOCKET_COUNT];
    memset( mediaSockPollArray, 0, sizeof(mediaSockPollArray) );

    while (m_RtpSession.isOpened() && !m_pleaseStop && !isDataTimerExpired())
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

        for (size_t i = 0; i < m_tracks.size(); ++i)
        {
            const TrackInfo& track = m_tracks[i];
            if(track.ioDevice)
            {
                if (!track.ioDevice->getMediaSocket())
                {
                    NX_ERROR(this, "%1: Invalid UDP socket handle", m_logName);
                    return QnAbstractMediaDataPtr();
                }
                mediaSockPollArray[nfds].fd = track.ioDevice->getMediaSocket()->handle();
                mediaSockPollArray[nfds++].events = POLLIN;
                fdToRtpIndex.push_back(i);
            }
        }
        mediaSockPollArray[nfds].fd = m_tcpSocket->handle();
        mediaSockPollArray[nfds++].events = POLLIN;

        const int rez = poll( mediaSockPollArray, nfds, MEDIA_DATA_READ_TIMEOUT_MS );
        if (rez < 1)
            continue;
        for( int i = 0; i < nfds; ++i )
        {
            if (!(mediaSockPollArray[i].revents & POLLIN ))
                continue;
            if (mediaSockPollArray[i].fd == m_tcpSocket->handle())
            {
                m_RtpSession.readAndProcessTextData();
                continue;
            }

            const int rtpChannelNum = fdToRtpIndex[i];
            TrackInfo& track = m_tracks[rtpChannelNum];
            while (!m_gotData)
            {
                quint8* rtpBuffer = QnRtspClient::prepareDemuxedData(m_demuxedData, rtpChannelNum, MAX_RTP_PACKET_SIZE); // todo: update here
                int bytesRead = track.ioDevice->read( (char*) rtpBuffer, MAX_RTP_PACKET_SIZE);
                if (bytesRead < 1)
                {
                    auto errorCode = SystemError::getLastOSErrorCode();
                    if (errorCode != SystemError::timedOut &&
                        errorCode != SystemError::again &&
                        errorCode != SystemError::wouldBlock &&
                        errorCode != SystemError::noError)
                    {
                        NX_DEBUG(this, "%1: Failed to read from UDP socket, OS code %2: %3",
                            m_logName, errorCode, SystemError::toString(errorCode));
                    }
                    break;
                }
                NX_VERBOSE(this, "%1: %2 bytes read form UDP socket", m_logName, bytesRead);
                m_demuxedData[rtpChannelNum]->finishWriting(bytesRead);
                quint8* bufferBase = (quint8*) m_demuxedData[rtpChannelNum]->data();
                int offset = rtpBuffer - bufferBase;

                if (!processData(track, bufferBase, offset, bytesRead, rtpChannelNum, errorRetryCount))
                    return QnAbstractMediaDataPtr(0);
            }
        }
    }

    return QnAbstractMediaDataPtr();
}

nx::rtp::StreamParserPtr RtspStreamProvider::createParser(const QString& codecName)
{
    if (codecName.isEmpty())
        return nullptr;

    using namespace nx::rtp;
    if (codecName.compare(QString::fromUtf8(kFfmpegCodecName), Qt::CaseInsensitive) == 0)
        return std::make_unique<nx::rtp::QnNxRtpParser>();

    if (codecName.compare(QString::fromUtf8(kFfmpegMetadataCodecName), Qt::CaseInsensitive) == 0)
        return std::make_unique<nx::rtp::NxRtpMetadataParser>();

    if (codecName == QLatin1String("H264"))
        return std::make_unique<nx::rtp::H264Parser>();

    if (codecName == QLatin1String("H265"))
        return std::make_unique<nx::rtp::HevcParser>();

    if (codecName == QLatin1String("JPEG"))
    {
        auto parser = std::make_unique<nx::rtp::MjpegParser>();

        // Forward configured resolution, that can be used when resolution more than 2048, and
        // camera not send any additional info.
        auto params = advancedLiveStreamParams();
        QSize& resolution = (m_role == Qn::CR_LiveVideo ? params.primaryStream.resolution :
            params.secondaryStream.resolution);
        parser->setConfiguredResolution(resolution.width(), resolution.height());
        return parser;
    }

    if (codecName == QLatin1String("MP4V-ES"))
        return std::make_unique<nx::rtp::Mpeg4Parser>();

    if (codecName == QLatin1String("MPEG4-GENERIC"))
        return std::make_unique<nx::rtp::AacParser>();

    if (codecName == QLatin1String("MPA"))
        return std::make_unique<nx::rtp::Mpeg12AudioParser>();

    if (codecName == QLatin1String("PCMU"))
        return std::make_unique<nx::rtp::SimpleAudioParser>(AV_CODEC_ID_PCM_MULAW);

    if (codecName == QLatin1String("PCMA"))
        return std::make_unique<nx::rtp::SimpleAudioParser>(AV_CODEC_ID_PCM_ALAW);

    if (codecName.startsWith(QLatin1String("G726"))) // g726-24, g726-32 e.t.c
    {
        int bitRatePos = codecName.indexOf(QLatin1Char('-'));
        if (bitRatePos == -1)
            return nullptr;
        QString bitsPerSample = codecName.mid(bitRatePos+1);
        auto parser = std::make_unique<nx::rtp::SimpleAudioParser>(AV_CODEC_ID_ADPCM_G726);
        parser->setBitsPerSample(bitsPerSample.toInt()/8);
        return parser;
    }

    if (codecName == QLatin1String("L16"))
        return std::make_unique<nx::rtp::SimpleAudioParser>(AV_CODEC_ID_PCM_S16BE);

    for (const auto& factory: m_customParserFactories)
    {
        if (auto parser = factory->createParser(codecName))
            return parser;
    }

    return nullptr;
}

nx::rtp::StreamParserPtr RtspStreamProvider::createParser(int payloadType)
{
    for (const auto& factory: m_customParserFactories)
    {
        if (auto parser = factory->createParser(payloadType))
            return parser;
    }

    return nullptr;
}

nx::vms::api::RtpTransportType RtspStreamProvider::getRtpTransport() const
{
    // Server side setting for resource.
    if (m_rtpTransport != nx::vms::api::RtpTransportType::automatic)
        return m_rtpTransport;

    NX_MUTEX_LOCKER lock(&defaultTransportMutex());
    return s_defaultTransportToUse;
}

void RtspStreamProvider::setRtpTransport(nx::vms::api::RtpTransportType value)
{
    m_rtpTransport = value;
}

void RtspStreamProvider::addCustomTrackParserFactory(
    std::unique_ptr<nx::rtp::IRtpParserFactory> parserFactory)
{
    m_customParserFactories.emplace_back(std::move(parserFactory));
}

void RtspStreamProvider::setExtraPayloadTypes(std::vector<int> payloadTypes)
{
    m_extraPayloadTypes = std::move(payloadTypes);
}

void RtspStreamProvider::setCloudConnectEnabled(bool value)
{
    m_cloudConnectEnabled = value;
}

std::chrono::microseconds RtspStreamProvider::translateTimestampFromCameraToVmsSystem(
    std::chrono::microseconds timestamp,
    int /*channelNumber*/)
{
    using namespace std::chrono;
    microseconds result;

    {
        NX_MUTEX_LOCKER lock(&m_tracksMutex);
        result = m_tracks.empty()
            ? microseconds::zero()
            : m_tracks[0].timeHelper->replayAdjustmentFromHistory(timestamp);
    }

    if (nx::utils::ini().checkCameraTimestamps)
    {
        const microseconds vmsSystemTimestamp = qnSyncTime->currentTimePoint();
        const seconds threshold(nx::utils::ini().cameraTimestampThresholdS);
        NX_ASSERT(timestamp >= vmsSystemTimestamp - threshold,
            NX_FMT("Camera ts %1us < VMS ts %2us by %3us (threshold %4s)",
                timestamp.count(),
                vmsSystemTimestamp.count(),
                (vmsSystemTimestamp - timestamp).count(),
                threshold.count()));
        NX_ASSERT(timestamp <= vmsSystemTimestamp + threshold,
            NX_FMT("Camera ts %1us > VMS ts %2us by %3us (threshold %4s)",
                timestamp.count(),
                vmsSystemTimestamp.count(),
                (timestamp - vmsSystemTimestamp).count(),
                threshold.count()));
        NX_ASSERT(result >= vmsSystemTimestamp - threshold,
            NX_FMT("Result ts %1us < VMS ts %2us by %3us (threshold %4s, camera ts %5us)",
                result.count(),
                vmsSystemTimestamp.count(),
                (vmsSystemTimestamp - result).count(),
                threshold.count(),
                timestamp.count()));
        NX_ASSERT(result <= vmsSystemTimestamp + threshold,
            NX_FMT("Result ts %1us > VMS ts %2us by %3us (threshold %4s, camera ts %5us)",
                result.count(),
                vmsSystemTimestamp.count(),
                (result - vmsSystemTimestamp).count(),
                threshold.count(),
                timestamp.count()));
    }

    return result;
}

std::optional<nx::network::http::Credentials> RtspStreamProvider::credentials() const
{
    return m_credentials;
}

void RtspStreamProvider::setCredentials(const nx::network::http::Credentials& credentials)
{
    m_credentials = credentials;
}

void  RtspStreamProvider::setUrl(const nx::utils::Url& url)
{
    m_url = url;
}

CameraDiagnostics::Result RtspStreamProvider::openStream()
{
    m_logName = m_url.toString() + " (Role: " + toString(m_role) + ")";
    m_pleaseStop = false;
    if (isStreamOpened())
        return CameraDiagnostics::NoErrorResult();
    m_gotSomeFrame = false;
    m_RtpSession.setTransport(getRtpTransport());
    m_RtpSession.setCloudConnectEnabled(m_cloudConnectEnabled);

    m_registeredMulticastAddresses.clear();
    updateStreamUrlIfNeeded();

    m_RtpSession.setCredentials(credentials().value_or(
        nx::network::http::Credentials()), m_preferredAuthScheme);

    {
        NX_MUTEX_LOCKER lock(&m_tracksMutex);
        m_tracks.clear();
    }
    m_trackIndices.clear();
    m_gotKeyDataInfo.clear();
    m_gotData = false;

    PlaybackRange playbackRange;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        std::swap(playbackRange, m_playbackRange);
        if (playbackRange.isValid())
        {
            NX_VERBOSE(
                this, "%1: Passing playback range %2 to RtpSession::open",
                __func__, playbackRange);
        }
    }

    m_openStreamResult = m_RtpSession.open(m_url, playbackRange.startTimeUsec);
    if(m_openStreamResult.errorCode != CameraDiagnostics::ErrorCode::noError)
        return m_openStreamResult;

    if (!m_customParserFactories.empty())
    {
        const auto tracks = m_RtpSession.getTrackInfo();
        std::set<QString> additionalSupportedCodecs;
        for (const QnRtspClient::SDPTrackInfo& track: tracks)
        {
            const QString& codecName = track.sdpMedia.rtpmap.codecName;
            if (isCodecSupportedByCustomParserFactories(codecName))
                additionalSupportedCodecs.insert(codecName);
        }

        m_RtpSession.setAdditionalSupportedCodecs(std::move(additionalSupportedCodecs));
    }

    if (!m_RtpSession.isPlayNowMode() && !m_RtpSession.sendSetup())
    {
        NX_WARNING(this, "Can't open RTSP stream [%1], SETUP request has been failed",
            m_url);

        m_openStreamResult = CameraDiagnostics::RequestFailedResult(m_url.toString(),
            "Can't open RTSP stream: SETUP request has been failed");
        return m_openStreamResult;
    }

    if (const auto result = registerMulticastAddressesIfNeeded(); !result)
    {
        m_openStreamResult = result;
        return m_openStreamResult;
    }

    if (!m_RtpSession.sendPlay(
        playbackRange.startTimeUsec,
        playbackRange.endTimeUsec,
        m_RtpSession.getScale()))
    {
        NX_WARNING(this, "Can't open RTSP stream [%1], PLAY request has been failed",
            m_url);

        m_openStreamResult = CameraDiagnostics::RequestFailedResult(m_url.toString(),
            "Can't open RTSP stream: PLAY request has been failed");
        return m_openStreamResult;
    }

    m_numberOfVideoChannels = numberOfVideoChannels();
    at_numberOfVideoChannelsChanged();

    createTrackParsers();

    m_openStreamResult = CameraDiagnostics::NoErrorResult();
    NX_DEBUG(this, "Successfully open RTSP stream %1", m_url);

    return m_openStreamResult;
}

void RtspStreamProvider::registerPredefinedTrack(int rtpChannelNumber)
{
    const int channelNumber = rtpChannelNumber / 2;
    auto codecParser = channelNumber == nx::rtp::kMetadataChannelNumber
        ? createParser("FFMPEG-METADATA")
        : createParser("FFMPEG");

    TrackInfo trackInfo;
    trackInfo.rtpParsers.emplace(
        nx::rtp::kNxPayloadType,
        std::make_unique<nx::rtp::RtpParser>(nx::rtp::kNxPayloadType, std::move(codecParser)));

        if (m_RtpSession.getActualTransport() == nx::vms::api::RtpTransportType::tcp)
            m_trackIndices[channelNumber] = rtpChannelNumber;

        NX_MUTEX_LOCKER lock(&m_tracksMutex);
        m_tracks.resize(std::max(m_tracks.size(), size_t(channelNumber + 1)));
        m_tracks[channelNumber] = std::move(trackInfo);
}

void RtspStreamProvider::createTrackParsers()
{
    using namespace nx::streaming;
    auto& trackInfo = m_RtpSession.getTrackInfo();
    int logicalVideoNum = 0;

    for (auto& track: trackInfo)
    {
        if (!track.setupSuccess || !track.ioDevice || !isFormatSupported(track.sdpMedia))
            continue;

        TrackInfo trackInfo;
        auto codecParser = createParser(track.sdpMedia.rtpmap.codecName.toUpper());
        if (!codecParser)
        {
            NX_WARNING(this, "Failed to create track parser for codec: [%1]",
                track.sdpMedia.rtpmap.codecName);
            continue;
        }
        codecParser->setLogId(m_logName.toStdString());
        codecParser->setSdpInfo(track.sdpMedia);
        auto audioParser = dynamic_cast<nx::rtp::AudioStreamParser*>(codecParser.get());
        if (audioParser)
        {
            auto codecParamters = audioParser->getCodecParameters();
            if (codecParamters)
                m_audioLayout.reset(new AudioLayout(codecParamters));
        }

        trackInfo.rtpParsers.emplace(
            track.sdpMedia.payloadType,
            std::make_unique<nx::rtp::RtpParser>(
                    track.sdpMedia.payloadType, std::move(codecParser)));

        for (int type: m_extraPayloadTypes)
        {
            if (auto parser = createParser(type))
            {
                parser->setSdpInfo(track.sdpMedia);
                trackInfo.rtpParsers.emplace(
                    type,
                    std::make_unique<nx::rtp::RtpParser>(type, std::move(parser)));
            }
        }

        trackInfo.ioDevice = track.ioDevice.get();
        trackInfo.rtcpChannelNumber = track.interleaved.second;
        trackInfo.ioDevice->setForceRtcpReports(isRtcpReportsForced());
        trackInfo.timeHelper = std::make_unique<nx::streaming::rtp::CameraTimeHelper>(timeHelperKey(), m_timeOffset);

        // Force camera time for NVR archives
        if (m_role == Qn::ConnectionRole::CR_Archive)
            trackInfo.timeHelper->setTimePolicy(nx::streaming::rtp::TimePolicy::forceCameraTime);
        else
            trackInfo.timeHelper->setTimePolicy(getTimePolicy());

        if (track.sdpMedia.mediaType == nx::rtp::Sdp::MediaType::Video)
            trackInfo.logicalChannelNum = logicalVideoNum++;
        else
            trackInfo.logicalChannelNum = m_numberOfVideoChannels;

        if (m_RtpSession.getActualTransport() == nx::vms::api::RtpTransportType::tcp)
            m_trackIndices[track.interleaved.first] = m_tracks.size();

        NX_MUTEX_LOCKER lock(&m_tracksMutex);
        m_tracks.emplace_back(std::move(trackInfo));
    }
}

nx::Mutex& RtspStreamProvider::defaultTransportMutex()
{
    static nx::Mutex s_defaultTransportMutex;
    return s_defaultTransportMutex;
}

void RtspStreamProvider::closeStream()
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

bool RtspStreamProvider::isStreamOpened() const
{
    return m_RtpSession.isOpened();
}

CameraDiagnostics::Result RtspStreamProvider::lastOpenStreamResult() const
{
    return m_openStreamResult;
}

AudioLayoutConstPtr RtspStreamProvider::getAudioLayout() const
{
    return m_audioLayout;
}

void RtspStreamProvider::pleaseStop()
{
    m_pleaseStop = true;
    m_RtpSession.shutdown();
}

void RtspStreamProvider::setDefaultTransport(nx::vms::api::RtpTransportType rtpTransport)
{
    NX_INFO(typeid(RtspStreamProvider), "Set default transport: %1", rtpTransport);

    NX_MUTEX_LOCKER lock(&defaultTransportMutex());
    s_defaultTransportToUse = rtpTransport;
}

void RtspStreamProvider::setRole(Qn::ConnectionRole role)
{
    m_role = role;
}

void RtspStreamProvider::setPreferredAuthScheme(
    const nx::network::http::header::AuthScheme::Value scheme)
{
    m_preferredAuthScheme = scheme;
}

QnConstResourceVideoLayoutPtr RtspStreamProvider::getVideoLayout() const
{
    NX_MUTEX_LOCKER lock( &m_layoutMutex );
    return m_customVideoLayout;
}

void RtspStreamProvider::setUserAgent(const QString& value)
{
    m_RtpSession.setUserAgent(value);
}

nx::utils::Url RtspStreamProvider::getCurrentStreamUrl() const
{
    return m_url;
}

void RtspStreamProvider::setPlaybackRange(int64_t startTimeUsec, int64_t endTimeUsec)
{
    return setPlaybackRange(PlaybackRange(startTimeUsec, endTimeUsec));
}

void RtspStreamProvider::setPlaybackRange(const PlaybackRange& playbackRange)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_playbackRange = playbackRange;
    NX_VERBOSE(this, "%1: Playback range set to %2", __func__, m_playbackRange);
}

void RtspStreamProvider::setDateTimeFormat(const QnRtspClient::DateTimeFormat& format)
{
    m_RtpSession.setDateTimeFormat(format);
}

void RtspStreamProvider::addRequestHeader(
    const QString& requestName,
    const nx::network::http::HttpHeader& header)
{
    m_RtpSession.addRequestHeader(requestName, header);
}

QnRtspClient& RtspStreamProvider::rtspClient()
{
    return m_RtpSession;
}

bool RtspStreamProvider::isFormatSupported(const nx::rtp::Sdp::Media media) const
{
    return media.mediaType == nx::rtp::Sdp::MediaType::Audio
        || media.mediaType == nx::rtp::Sdp::MediaType::Video
        || isCodecSupportedByCustomParserFactories(media.rtpmap.codecName);
}

void RtspStreamProvider::setOnSocketReadTimeoutCallback(
    std::chrono::milliseconds timeout,
    OnSocketReadTimeoutCallback callback)
{
    m_callbackTimeout = timeout;
    m_onSocketReadTimeoutCallback = std::move(callback);
}

void RtspStreamProvider::setRtpFrameTimeoutMs(int value)
{
    m_rtpFrameTimeoutMs = value;
}

// ------------------------------------- RtspResourceStreamProvider -----------------------------------

RtspResourceStreamProvider::RtspResourceStreamProvider(
    const QnVirtualCameraResourcePtr& resource,
    const nx::streaming::rtp::TimeOffsetPtr& timeOffset)
    :
    RtspStreamProvider(resource->systemContext()->globalSettings(), timeOffset),
    m_resource(resource)
{
    const bool qopValue = resource->resourceData().value<bool>("ignoreQopInDigestAuth");
    m_RtpSession.setIgnoreQopInDigest(qopValue);

    m_ignoreRtcpReports = resource->resourceData().value<bool>(
        ResourceDataKey::kIgnoreRtcpReports, m_ignoreRtcpReports);
}

void RtspResourceStreamProvider::updateTimePolicy()
{
    if (m_role != Qn::ConnectionRole::CR_Archive)
    {
        NX_MUTEX_LOCKER lock(&m_tracksMutex);
        for (auto& track: m_tracks)
        {
            track.timeHelper->setTimePolicy(getTimePolicy());
            track.timeHelper->resetBadCameraTimeState();
        }
    }
}

nx::vms::api::RtpTransportType RtspResourceStreamProvider::getRtpTransport() const
{
    // Client defined settings for resource.
    if (m_resource)
    {
        const auto rtpTransportString =
            m_resource->getProperty(QnMediaResource::rtpTransportKey());

        const auto transport = nx::reflect::fromString<nx::vms::api::RtpTransportType>(
            rtpTransportString.toStdString(),
            nx::vms::api::RtpTransportType::automatic);

        if (transport != nx::vms::api::RtpTransportType::automatic)
            return transport;
    }
    return base_type::getRtpTransport();
}

CameraDiagnostics::Result RtspResourceStreamProvider::openStream()
{
    setUrl(m_resource->getUrl());

    // Set credentials from resource only if not already forced.
    if (!m_credentials)
        setCredentials(nx::network::http::Credentials(m_resource->getAuth()));

    m_RtpSession.setMediaTypeEnabled(
        nx::rtp::Sdp::MediaType::Audio,
        m_resource->isAudioRequired());
    m_RtpSession.setMediaTypeEnabled(
        nx::rtp::Sdp::MediaType::Metadata,
        m_resource->isRtspMetatadaRequired());

    auto result = base_type::openStream();
    if (result.errorCode != CameraDiagnostics::ErrorCode::noError)
        return result;

    bool videoExist = false;
    bool audioExist = false;
    for (const auto& track : m_RtpSession.getTrackInfo())
    {
        videoExist |= track.sdpMedia.mediaType == nx::rtp::Sdp::MediaType::Video;
        audioExist |= track.sdpMedia.mediaType == nx::rtp::Sdp::MediaType::Audio;
    }

    if (m_role == Qn::CR_LiveVideo)
    {
        if (audioExist) {
            if (!m_resource->isAudioSupported())
                m_resource->forceEnableAudio();
        }
        else {
            m_resource->forceDisableAudio();
        }
    }

    m_rtcpReportTimer.restart();
    if (!audioExist && !videoExist)
    {
        if (isEmptyMediaAllowed(m_resource))
        {
            m_rtpFrameTimeoutMs = -1;
            m_RtpSession.setTCPTimeout(m_RtpSession.keepAliveTimeOut());
        }
        else
        {
            m_RtpSession.stop();
            m_openStreamResult = CameraDiagnostics::NoMediaTrackResult(m_url);
            return m_openStreamResult;
        }
    }
    m_openStreamResult = CameraDiagnostics::NoErrorResult();
    return CameraDiagnostics::NoErrorResult();
}

nx::streaming::rtp::TimePolicy RtspStreamProvider::getTimePolicy() const
{
    return nx::streaming::rtp::TimePolicy::forceCameraTime;
}

CameraDiagnostics::Result RtspStreamProvider::registerMulticastAddressesIfNeeded()
{
    // Multicast is not supported for the base class. It is needed to refactor MulticastRegistry
    // class if it is will be required in the future.
    return CameraDiagnostics::NoErrorResult();
}

int RtspStreamProvider::numberOfVideoChannels() const
{
    return m_RtpSession.getTrackCount(nx::rtp::Sdp::MediaType::Video);
}

bool RtspStreamProvider::isRtcpReportsForced() const
{
    return false;
}

std::string RtspStreamProvider::timeHelperKey() const
{
    return m_url.toStdString();
}

void RtspStreamProvider::setForceCameraTime(bool value)
{
    m_forceCameraTime = value;
}

// ------------------------- RtspResourceStreamProvider -------------------

int RtspResourceStreamProvider::numberOfVideoChannels() const
{
    if (!m_resource->allowRtspVideoLayout())
        return 1;
    return base_type::numberOfVideoChannels();
}

void RtspResourceStreamProvider::at_numberOfVideoChannelsChanged()
{
    QString manualConfiguredLayout = getConfiguredVideoLayout(m_resource);
    if (m_numberOfVideoChannels > 1 && manualConfiguredLayout.isEmpty())
    {
        NX_MUTEX_LOCKER lock(&m_layoutMutex);
        QString newVideoLayout;
        m_customVideoLayout = QnCustomResourceVideoLayoutPtr(
            new QnCustomResourceVideoLayout(QSize(m_numberOfVideoChannels, 1)));
        for (int i = 0; i < m_numberOfVideoChannels; ++i)
            m_customVideoLayout->setChannel(i, 0, i); // arrange multi video layout from left to right

        newVideoLayout = m_customVideoLayout->toString();
        if (m_role == Qn::CR_LiveVideo &&
            m_resource->setProperty(ResourcePropertyKey::kVideoLayout, newVideoLayout))
        {
            m_resource->saveProperties();
        }
    }
}

bool RtspResourceStreamProvider::isRtcpReportsForced() const
{
    auto resData = m_resource->resourceData();
    return resData.value<bool>(lit("forceRtcpReports"), false);
}

std::string RtspResourceStreamProvider::timeHelperKey() const
{
    return m_resource->getPhysicalId().toStdString();
}

void RtspResourceStreamProvider::updateStreamUrlIfNeeded()
{
    int mediaPort = m_resource->mediaPort();
    if (m_request.startsWith("rtsp://") || m_request.startsWith("rtsps://"))
    {
        m_url = m_request;
        if (mediaPort)
            m_url.setPort(mediaPort); //< Override port.
    }
    else
    {
        m_url.clear();
        m_url.setScheme("rtsp");
        m_url.setHost(m_resource->getHostAddress());
        m_url.setPort(mediaPort ? mediaPort : nx::network::rtsp::DEFAULT_RTSP_PORT);

        if (!m_request.isEmpty())
        {
            auto requestParts = m_request.split('?');
            QString path = requestParts[0];
            if (!path.startsWith('/'))
                path.insert(0, '/');
            m_url.setPath(path);
            if (requestParts.size() > 1)
                m_url.setQuery(requestParts[1]);
        }
    }
}

nx::streaming::rtp::TimePolicy RtspResourceStreamProvider::getTimePolicy() const
{
    if (m_resource->trustCameraTime())
        return nx::streaming::rtp::TimePolicy::useCameraTimeIfCorrect;

    return nx::streaming::rtp::TimePolicy::bindCameraTimeToLocalTime;
}

CameraDiagnostics::Result RtspResourceStreamProvider::registerMulticastAddressesIfNeeded()
{
    auto tracks = m_RtpSession.getTrackInfo();
    if (tracks.empty())
    {
        NX_WARNING(this, "%1: Tracks are empty for [%2]", m_logName, m_url);
        return CameraDiagnostics::CameraInvalidParams("The list of tracks are empty");
    }

    std::set<QnRtspIoDevice::AddressInfo> addressInfoToRegister;
    for (const auto& track : tracks)
    {
        if (!track.ioDevice || !track.setupSuccess)
            continue;

        addressInfoToRegister.insert(track.ioDevice->mediaAddressInfo());
        addressInfoToRegister.insert(track.ioDevice->rtcpAddressInfo());
    }

    for (const auto& addressInfo : addressInfoToRegister)
    {
        if (!registerAddressIfNeeded(addressInfo))
        {
            return CameraDiagnostics::CameraInvalidParams(
                "Multicast media address conflict detected");
        }
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result RtspResourceStreamProvider::registerAddressIfNeeded(
    const QnRtspIoDevice::AddressInfo& addressInfo)
{
    if (addressInfo.transport != nx::vms::api::RtpTransportType::multicast)
        return CameraDiagnostics::NoErrorResult();

    const auto& multicastAddress = addressInfo.address;
    if (!multicastAddress.address.isMulticast()
        || multicastAddress.port <= 0)
    {
        reportNetworkIssue(
            qnSyncTime->currentTimePoint(),
            vms::api::EventReason::networkMulticastAddressIsInvalid,
            nx::vms::rules::NetworkIssueInfo{.address = multicastAddress});

        return CameraDiagnostics::CameraInvalidParams(
            "Address %1 is not a multicast address, though multicast transport has been chosen");
    }

    auto multicastAddressRegistry = this->multicastAddressRegistry();
    if (!multicastAddressRegistry)
    {
        return CameraDiagnostics::InternalServerErrorResult(
            "Unable to access multicast address registry");
    }

    auto registeredAddress = multicastAddressRegistry->registerAddress(
        m_resource,
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

        nx::vms::rules::NetworkIssueInfo info = {
            .address = multicastAddress,
            .deviceName = deviceName,
            .stream = addressUsageInfo.stream,
        };

        reportNetworkIssue(
            qnSyncTime->currentTimePoint(),
            vms::api::EventReason::networkMulticastAddressConflict,
            info);

        return CameraDiagnostics::CameraInvalidParams(
            nx::format("Multicast address %1 is already in use by %2")
                .args(multicastAddress, deviceName));
    }

    return CameraDiagnostics::NoErrorResult();
}

} // namespace nx::streaming
