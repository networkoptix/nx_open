#include <QtCore/QUrlQuery>

#include <chrono>

#include "hanwha_stream_reader.h"
#include "hanwha_resource.h"
#include "hanwha_request_helper.h"
#include "hanwha_utils.h"
#include "hanwha_common.h"
#include "hanwha_chunk_loader.h"
#include "vms_server_hanwha_ini.h"

#include <utils/common/sleep.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/log/log.h>
#include <utils/common/util.h>

namespace nx {
namespace vms::server {
namespace plugins {

namespace {

static const QString kLive4NvrProfileName = lit("Live4NVR");
static const QString kTooManyConnectionsMessagePattern("maximum");
static const int kHanwhaDefaultPrimaryStreamProfile = 2;
static const std::chrono::milliseconds kNvrSocketReadTimeout(500);
static const std::chrono::milliseconds kTimeoutToExtrapolateTime(1000 * 5);

static const QString kHanwhaTcp("TCP");
static const QString kHanwhaUdp("UDP");

QString toHanwhaStreamingType(nx::vms::api::RtpTransportType rtpTransport)
{
    using namespace nx::vms::api;
    if (rtpTransport == nx::vms::api::RtpTransportType::multicast)
        return kHanwhaRtpMulticast;

    return kHanwhaRtpUnicast;
}

QString toHanwhaTransportProtocol(nx::vms::api::RtpTransportType rtpTransport)
{
    using namespace nx::vms::api;
    switch (rtpTransport)
    {
        case RtpTransportType::automatic:
        case RtpTransportType::tcp:
            return kHanwhaTcp;
        case RtpTransportType::udp:
        case RtpTransportType::multicast:
            return kHanwhaUdp;
        default:
            NX_ASSERT(false, "Invalid RTP transport");
            return kHanwhaTcp;
    }
}

} // namespace

HanwhaStreamReader::HanwhaStreamReader(
    const HanwhaResourcePtr& res)
    :
    QnRtpStreamReader(res),
    m_hanwhaResource(res)
{
}

HanwhaStreamReader::~HanwhaStreamReader()
{
}

CameraDiagnostics::Result HanwhaStreamReader::openStreamInternal(
    bool isCameraControlRequired,
    const QnLiveStreamParams& params)
{
    if (isCameraControlRequired && !m_hanwhaResource->isNvr())
    {
        auto result = updateProfile(params);
        if (!result)
            return result;
    }

    QString streamUrlString;
    auto result = streamUri(&streamUrlString);
    if (!result)
        return result;

    if (m_hanwhaResource->isNvr())
    {
        QnUuid clientId;
        if (m_sessionType != HanwhaSessionType::live)
            clientId = m_clientId;
        m_sessionContext = m_hanwhaResource->session(m_sessionType, clientId);
        if (m_sessionContext.isNull())
            return CameraDiagnostics::TooManyOpenedConnectionsResult();

        // QUrl isn't used here intentionally, since session parameter works correct
        // if it added to the end of URL only.
        streamUrlString.append(lit("/session=%1").arg(m_sessionContext->sessionId));
    }

    const auto role = getRole();
    m_rtpReader.setRole(role);
    m_rtpReader.setDateTimeFormat(QnRtspClient::DateTimeFormat::ISO);
    if (m_hanwhaResource->isNvr() && m_sessionType == HanwhaSessionType::archive)
    {
        m_rtpReader.setOnSocketReadTimeoutCallback(
            kNvrSocketReadTimeout,
            [this](){ return createEmptyPacket(); });
        m_rtpReader.setRtpFrameTimeoutMs(std::numeric_limits<int>::max()); //< Media frame timeout
    }

    if (!m_rateControlEnabled)
        m_rtpReader.addRequestHeader(lit("PLAY"), nx::network::http::HttpHeader("Rate-Control", "no"));

    m_rtpReader.setRequest(streamUrlString);
    m_hanwhaResource->updateSourceUrl(streamUrlString, role);

    const auto openResult = m_rtpReader.openStream();
    NX_DEBUG(this, lm("Open RTSP %1 for device %2").args(
        streamUrlString, m_resource->getUniqueId()));
    if (!openResult &&
        openResult.toString(resourcePool()).toLower().contains(kTooManyConnectionsMessagePattern))
    {
        return CameraDiagnostics::TooManyOpenedConnectionsResult();
    }
    return openResult;
}

void HanwhaStreamReader::closeStream()
{
    base_type::closeStream();
    m_sessionContext.reset();
}

CameraDiagnostics::Result HanwhaStreamReader::updateProfile(const QnLiveStreamParams& parameters)
{
    if (!m_hanwhaResource->isConnectedViaSunapi())
        return CameraDiagnostics::NoErrorResult();

    const auto role = getRole();
    NX_ASSERT(role != Qn::ConnectionRole::CR_Archive);
    if (role == Qn::ConnectionRole::CR_Archive)
        return CameraDiagnostics::NoErrorResult();

    const auto profileNumber = m_hanwhaResource->profileByRole(role);
    if (!isCorrectProfile(profileNumber))
    {
        return CameraDiagnostics::CameraInvalidParams(
            lit("Update profile: invalid profile number is given"));
    }

    HanwhaRequestHelper helper(m_hanwhaResource->sharedContext());
    HanwhaProfileParameterFlags profileParameterFlags;
    if (m_hanwhaResource->isAudioSupported())
        profileParameterFlags |= HanwhaProfileParameterFlag::audioSupported;

    const auto profileParameters = m_hanwhaResource->makeProfileParameters(
        getRole(),
        parameters,
        profileParameterFlags);

    // Some cameras have bug: they could close connection with delay.
    // It affects newly opened connections. Don't change parameters to prevent it.
    if (m_prevProfileParameters == profileParameters)
        return CameraDiagnostics::NoErrorResult();

    m_hanwhaResource->beforeConfigureStream(getRole());

    auto response = helper.update(lit("media/videoprofile"), profileParameters);
    if (!response.isSuccessful() && response.errorCode() == kHanwhaInvalidParameterHttpCode)
    {
        // It is a camera bug workaround.
        // We have at least 1 camera that report supported audio but it is really not.
        const auto profileParameters = m_hanwhaResource->makeProfileParameters(
            getRole(),
            parameters,
            HanwhaProfileParameterFlags());
        response = helper.update(lit("media/videoprofile"), profileParameters);
    }

    m_hanwhaResource->afterConfigureStream(getRole());
    if (!response.isSuccessful())
    {
        return error(
            response,
            CameraDiagnostics::RequestFailedResult(response.requestUrl(), response.errorString()));
    }

    m_prevProfileParameters = profileParameters;
    return CameraDiagnostics::NoErrorResult();
}

bool HanwhaStreamReader::isCorrectProfile(int profileNumber) const
{
    return profileNumber != kHanwhaInvalidProfile
        || getRole() == Qn::CR_Archive;
}

CameraDiagnostics::Result HanwhaStreamReader::streamUri(QString* outUrl)
{
    const auto forcedUrlString = forcedUrl(getRole());
    if (!forcedUrlString.isEmpty())
    {
        *outUrl = forcedUrlString;
        return CameraDiagnostics::NoErrorResult();
    }

    const nx::vms::api::RtpTransportType preferredRtpTransport =
        m_hanwhaResource->preferredRtpTransport();

    using ParameterMap = std::map<QString, QString>;
    ParameterMap params =
    {
        {kHanwhaChannelProperty, QString::number(m_hanwhaResource->getChannel())},
        {kHanwhaStreamingModeProperty, kHanwhaFullMode},
        {kHanwhaStreamingTypeProperty, toHanwhaStreamingType(preferredRtpTransport)},
        {kHanwhaTransportProtocolProperty, toHanwhaTransportProtocol(preferredRtpTransport)},
        {kHanwhaRtspOverHttpProperty, kHanwhaFalse}
    };

    const auto role = getRole();
    if (role == Qn::CR_Archive)
    {
        const auto mediaType = m_hanwhaResource->isNvr()
            ? kHanwhaSearchMediaType
            : kHanwhaBackupMediaType;

        if (m_hanwhaResource->isNvr())
        {
            m_overlappedId = m_hanwhaResource->sharedContext()->overlappedId();
            if (m_overlappedId == boost::none)
            {
                return CameraDiagnostics::CameraInvalidParams(
                    lit("Unknown current overlapped ID."));
            }
            params.emplace(
                kHanwhaOverlappedIdProperty,
                QString::number(*m_overlappedId));
        }

        params.emplace(kHanwhaMediaTypeProperty, mediaType);
    }
    else
    {
        params.emplace(kHanwhaMediaTypeProperty, kHanwhaLiveMediaType);
    }

    if (m_hanwhaResource->isNvr())
        params.emplace(kHanwhaClientTypeProperty, "PC");

    const auto profileNumber = m_hanwhaResource->profileByRole(role);
    if (profileNumber != kHanwhaInvalidProfile)
        params.emplace(kHanwhaProfileNumberProperty, QString::number(profileNumber));

    HanwhaRequestHelper helper(m_hanwhaResource->sharedContext());
    const auto response = helper.view(lit("media/streamuri"), params);
    if (!response.isSuccessful())
    {
        return error(
            response,
            CameraDiagnostics::RequestFailedResult(
                response.requestUrl(),
                response.errorString()));
    }

    auto rtspUri = response.response()[kHanwhaUriProperty];
    *outUrl = m_hanwhaResource->fromOnvifDiscoveredUrl(rtspUri.toStdString(), false);

    if (!m_hanwhaResource->isNvr() && role == Qn::ConnectionRole::CR_Archive)
    {
        NX_ASSERT(m_overlappedId != boost::none);
        if (m_overlappedId == boost::none)
        {
            return CameraDiagnostics::CameraInvalidParams(
                lit("No overlapped id is set for archive export."));
        }

        QUrl url(*outUrl);
        url.setPath(lit("/recording/OverlappedID=%1/play.smp")
            .arg(*m_overlappedId));

        *outUrl = url.toString();
        NX_VERBOSE(this) << *outUrl << "Time Range:"
            << QDateTime::fromMSecsSinceEpoch(m_startTimeUsec / 1000)
            << QDateTime::fromMSecsSinceEpoch(m_endTimeUsec / 1000);
    }

    if (ini().forcedRtspPort > 0)
    {
        QUrl realUrl(*outUrl);
        realUrl.setPort(ini().forcedRtspPort);
        *outUrl = realUrl.toString();
    }

    return CameraDiagnostics::NoErrorResult();
}

QString HanwhaStreamReader::rtpTransport() const
{
    return toString(m_rtpReader.getRtpTransport());
}

void HanwhaStreamReader::setPositionUsec(qint64 value)
{
    m_lastTimestampUsec = value;
    m_timeSinceLastFrame.invalidate();

    if (ini().enableSingleSeekPerGroup)
    {
        // Send single seek for all channels
        static QnMutex sessionMutex;
        QnMutexLocker lock(&sessionMutex);
        if (m_sessionContext)
        {
            SeekPosition newPosition(value);
            if (m_rtpReader.isStreamOpened()
                && m_sessionContext->lastSeekPos.canJoinPosition(newPosition))
            {
                return;
            }
            m_sessionContext->lastSeekPos = newPosition;
        }
    }

    NX_ASSERT(value != AV_NOPTS_VALUE && value != DATETIME_NOW);
    NX_DEBUG(this, lm("Set position %1 for device %2").args(mksecToDateTime(value), m_resource->getUniqueId()));
    m_rtpReader.setPositionUsec(value);
}

void HanwhaStreamReader::setRateControlEnabled(bool enabled)
{
    m_rateControlEnabled = enabled;
}

void HanwhaStreamReader::setPlaybackRange(int64_t startTimeUsec, int64_t endTimeUsec)
{
    m_startTimeUsec = startTimeUsec;
    m_endTimeUsec = endTimeUsec;
}

void HanwhaStreamReader::setOverlappedId(nx::core::resource::OverlappedId overlappedId)
{
    m_overlappedId = overlappedId;
}

SessionContextPtr HanwhaStreamReader::sessionContext()
{
    return m_sessionContext;
}

QnRtspClient& HanwhaStreamReader::rtspClient()
{
    return m_rtpReader.rtspClient();
}

void HanwhaStreamReader::setSessionType(HanwhaSessionType value)
{
    m_sessionType = value;
}

void HanwhaStreamReader::setClientId(const QnUuid& id)
{
    m_clientId = id;
}

QnAbstractMediaDataPtr HanwhaStreamReader::createEmptyPacket()
{
    NX_ASSERT(m_hanwhaResource->isNvr());
    if (!m_hanwhaResource->isNvr())
        return QnAbstractMediaDataPtr();


    const auto context = m_hanwhaResource->sharedContext();
    const int speed = m_rtpReader.rtspClient().getScale();
    qint64 currentTimeMs = m_lastTimestampUsec / 1000;

    if (ini().enableArchivePositionExtrapolation)
    {
        // Extrapolate current position if NVR doesn't send packets during some period.
        // It is needed for sync play mode.
        if (m_timeSinceLastFrame.isValid())
            currentTimeMs += m_timeSinceLastFrame.elapsedMs() * speed;
        const bool isForwardSearch = speed >= 0;
        const auto timeline = context->overlappedTimeline(m_hanwhaResource->getChannel());
        NX_ASSERT(timeline.size() <= 1, lit("There should be only one overlapped ID for NVRs"));
        if (timeline.size() != 1)
            return QnAbstractMediaDataPtr();

        const auto chunks = timeline.cbegin()->second;
        if (chunks.containTime(currentTimeMs))
        {
            if (m_timeSinceLastFrame.isValid()
                && m_timeSinceLastFrame.elapsed() < kTimeoutToExtrapolateTime)
            {
                return QnAbstractMediaDataPtr(); //< Don't forecast position too fast.
            }
        }
        else
        {
            auto itr = chunks.findNearestPeriod(currentTimeMs, isForwardSearch);
            if (itr == chunks.end())
                currentTimeMs = isForwardSearch ? DATETIME_NOW : 0;
            else
                currentTimeMs = isForwardSearch ? itr->startTimeMs : itr->endTimeMs();
        }
    }

    QnAbstractMediaDataPtr rez(new QnEmptyMediaData());
    rez->timestamp = currentTimeMs * 1000;
    if (speed < 0)
        rez->flags |= QnAbstractMediaData::MediaFlags_Reverse;

    NX_VERBOSE(this, lm("Create extrapolation packet with time %1 (base time %2) for device %3").args(
        mksecToDateTime(rez->timestamp),
        mksecToDateTime(m_lastTimestampUsec),
        m_resource->getUniqueId()));

    return rez;
}

QString HanwhaStreamReader::forcedUrl(Qn::ConnectionRole role) const
{
    switch (role)
    {
        case Qn::ConnectionRole::CR_LiveVideo:
            return QString::fromUtf8(ini().forcedPrimaryStreamUrl).trimmed();
        case Qn::ConnectionRole::CR_SecondaryLiveVideo:
            return QString::fromUtf8(ini().forcedSecondaryStreamUrl).trimmed();
        default:
            return QString();
    }
}

QnAbstractMediaDataPtr HanwhaStreamReader::getNextData()
{
    auto result = base_type::getNextData();
    if (result && result->dataType != QnAbstractMediaData::EMPTY_DATA)
    {
        m_lastTimestampUsec = result->timestamp;
        m_timeSinceLastFrame.restart();

        NX_VERBOSE(this, lm("GOT RTSP packet with time %1 for device %2").args(
            mksecToDateTime(result->timestamp), m_resource->getUniqueId()));
    }

    return result;
}

} // namespace plugins
} // namespace vms::server
} // namespace nx
