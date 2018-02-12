#include <QtCore/QUrlQuery>

#include <chrono>

#include "hanwha_stream_reader.h"
#include "hanwha_resource.h"
#include "hanwha_request_helper.h"
#include "hanwha_utils.h"
#include "hanwha_common.h"
#include "hanwha_chunk_reader.h"

#include <utils/common/sleep.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

static const QString kLive4NvrProfileName = lit("Live4NVR");
static const int kHanwhaDefaultPrimaryStreamProfile = 2;
static const int kNvrSocketReadTimeoutMs = 500;
static const std::chrono::milliseconds kTimeoutToExtrapolateTimeMs(1000 * 3);

} // namespace

HanwhaStreamReader::HanwhaStreamReader(const HanwhaResourcePtr& res):
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
    const auto role = getRole();
    QString streamUrlString;

    int profileToOpen = kHanwhaInvalidProfile;
    if (m_hanwhaResource->isConnectedViaSunapi())
        profileToOpen = m_hanwhaResource->profileByRole(role);
    else
        profileToOpen = chooseNvrChannelProfile(role);

    if (!isCorrectProfile(profileToOpen))
    {
        return CameraDiagnostics::CameraInvalidParams(
            lit("No profile for %1 stream").arg(
                role == Qn::ConnectionRole::CR_LiveVideo
                ? lit("primary")
                : lit("secondary")));
    }

    if (isCameraControlRequired)
    {
        auto result = updateProfile(profileToOpen, params);
        if (!result)
            return result;
    }

    auto result = streamUri(profileToOpen, &streamUrlString);
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

        streamUrlString.append(lit("&session=%1").arg(m_sessionContext->sessionId));
    }

    m_rtpReader.setDateTimeFormat(QnRtspClient::DateTimeFormat::ISO);
    m_rtpReader.setRole(role);
    if (m_hanwhaResource->isNvr() && m_sessionType == HanwhaSessionType::archive)
    {
        m_rtpReader.rtspClient().setTCPTimeout(kNvrSocketReadTimeoutMs);
        m_rtpReader.setOnSocketReadTimeoutCallback([this](){ return createEmptyPacket(); });
        m_rtpReader.setRtpFrameTimeoutMs(std::numeric_limits<int>::max()); //< Media frame timeout
    }

    if (m_hanwhaResource->isNvr())
        m_rtpReader.setTimePolicy(TimePolicy::forceCameraTime);
    else if (role == Qn::ConnectionRole::CR_Archive)
        m_rtpReader.setTimePolicy(TimePolicy::onvifExtension);
    else
        m_rtpReader.setTimePolicy(TimePolicy::ignoreCameraTimeIfBigJitter);

    if (!m_rateControlEnabled)
        m_rtpReader.addRequestHeader(lit("PLAY"), nx_http::HttpHeader("Rate-Control", "no"));

    m_rtpReader.setRequest(streamUrlString);
    m_hanwhaResource->updateSourceUrl(streamUrlString, role);

    const auto openResult = m_rtpReader.openStream();
    NX_VERBOSE(this, lm("RTP open %1 -> %2 (client id %3)").args(
        streamUrlString, openResult.toString(nullptr), m_clientId));

    return openResult;
}

void HanwhaStreamReader::closeStream()
{
    base_type::closeStream();
    m_sessionContext.reset();
}

HanwhaProfileParameters HanwhaStreamReader::makeProfileParameters(
    int profileNumber,
    const QnLiveStreamParams& parameters) const
{
    const auto role = getRole();
    const auto codec = m_hanwhaResource->streamCodec(role);
    const auto codecProfile = m_hanwhaResource->streamCodecProfile(codec, role);
    const auto resolution = m_hanwhaResource->streamResolution(role);
    const auto frameRate = m_hanwhaResource->streamFrameRate(role, parameters.fps);
    const auto govLength = m_hanwhaResource->streamGovLength(role);
    const auto bitrateControl = m_hanwhaResource->streamBitrateControl(role); //< cbr/vbr
    const auto bitrate = m_hanwhaResource->streamBitrate(role, parameters);

    const auto govLengthParameterName =
        lit("%1.GOVLength").arg(toHanwhaString(codec));

    const auto bitrateControlParameterName =
        lit("%1.BitrateControlType").arg(toHanwhaString(codec));

    const bool isH26x = codec == AVCodecID::AV_CODEC_ID_H264
        || codec == AVCodecID::AV_CODEC_ID_HEVC;

    HanwhaProfileParameters result =
    {
        {kHanwhaChannelProperty, QString::number(m_hanwhaResource->getChannel())},
        {kHanwhaProfileNumberProperty, QString::number(profileNumber)},
        {kHanwhaEncodingTypeProperty, toHanwhaString(codec)},
        {kHanwhaResolutionProperty, toHanwhaString(resolution)}
    };

    if (m_hanwhaResource->isAudioSupported())
    {
        result.emplace(kHanwhaAudioInputEnableProperty, toHanwhaString(
            m_hanwhaResource->isAudioEnabled()));
    }

    if (isH26x)
    {
        if (govLength != kHanwhaInvalidGovLength)
            result.emplace(govLengthParameterName, QString::number(govLength));
        if (!codecProfile.isEmpty())
            result.emplace(lit("%1.Profile").arg(toHanwhaString(codec)), codecProfile);
    }

    if (isH26x && bitrateControl != Qn::BitrateControl::undefined)
        result.emplace(bitrateControlParameterName, toHanwhaString(bitrateControl));

    if (bitrate != kHanwhaInvalidBitrate)
        result.emplace(kHanwhaBitrateProperty, QString::number(bitrate));

    if (frameRate != kHanwhaInvalidFps)
        result.emplace(kHanwhaFrameRatePriority, QString::number(frameRate));

    return result;
}

CameraDiagnostics::Result HanwhaStreamReader::updateProfile(
    int profileNumber,
    const QnLiveStreamParams& parameters)
{
    if (!m_hanwhaResource->isConnectedViaSunapi())
        return CameraDiagnostics::NoErrorResult();

    if (profileNumber == kHanwhaInvalidProfile)
    {
        return CameraDiagnostics::CameraPluginErrorResult(
            lit("Update profile: invalid profile number is given"));
    }

    HanwhaRequestHelper helper(m_hanwhaResource->sharedContext());
    helper.setIgnoreMutexAnalyzer(true);
    const auto profileParameters = makeProfileParameters(profileNumber, parameters);
    // Some cameras have bug: they could close connection with delay.
    // It affects newly opened connections. Don't change parameters to prevent it.
    if (m_prevProfileParameters == profileParameters)
        return CameraDiagnostics::NoErrorResult();

    m_hanwhaResource->beforeConfigureStream(getRole());
    const auto response = helper.update(lit("media/videoprofile"), profileParameters);
    m_hanwhaResource->afterConfigureStream(getRole());
    if (!response.isSuccessful())
    {
        return error(
            response,
            CameraDiagnostics::RequestFailedResult(
                response.requestUrl(),
                response.errorString()));
    }

    m_prevProfileParameters = profileParameters;
    return CameraDiagnostics::NoErrorResult();
}

QSet<int> HanwhaStreamReader::availableProfiles(int channel) const
{
    QSet<int> result;
    HanwhaRequestHelper helper(m_hanwhaResource->sharedContext());
    helper.setIgnoreMutexAnalyzer(true);
    const auto response = helper.view(
        lit("media/videoprofilepolicy"),
        {{ kHanwhaChannelProperty, QString::number(channel) }});

    if (!response.isSuccessful())
        return result;

    for (const auto& entry: response.response())
    {
        if (entry.first.endsWith("Profile"))
            result << entry.second.toInt();
    }
    return result;
}

int HanwhaStreamReader::chooseNvrChannelProfile(Qn::ConnectionRole role) const
{
    const auto channel = m_hanwhaResource->getChannel();

    HanwhaRequestHelper helper(m_hanwhaResource->sharedContext());
    helper.setIgnoreMutexAnalyzer(true);
    const auto response = helper.view(
        lit("media/videoprofile"),
        {{kHanwhaChannelProperty, QString::number(channel)}});

    if (!response.isSuccessful())
        return kHanwhaInvalidProfile;

    const auto profiles = parseProfiles(response);
    if (profiles.empty())
        return kHanwhaInvalidProfile;

    if (profiles.find(channel) == profiles.cend())
        return kHanwhaInvalidProfile;

    const auto& channelProfiles = profiles.at(channel);

    if (channelProfiles.empty())
        return kHanwhaInvalidProfile;

    int bestProfile = kHanwhaInvalidProfile;
    int bestScore = 0;
    const auto profileFilter = availableProfiles(channel);

    for (const auto& profileEntry: channelProfiles)
    {
        const auto profile = profileEntry.second;
        const auto codecCoefficient =
            kHanwhaCodecCoefficients.find(profile.codec) != kHanwhaCodecCoefficients.cend()
            ? kHanwhaCodecCoefficients.at(profile.codec)
            : -1;

        if (!profileFilter.contains(profile.number))
            continue;

        const auto score = profile.resolution.width()
            * profile.resolution.height()
            * codecCoefficient
            + profile.frameRate;

        if (score > bestScore)
        {
            bestScore = score;
            bestProfile = profile.number;
        }
    }

    return bestProfile;
}

bool HanwhaStreamReader::isCorrectProfile(int profileNumber) const
{
    return profileNumber != kHanwhaInvalidProfile
        || getRole() == Qn::CR_Archive;
}

CameraDiagnostics::Result HanwhaStreamReader::streamUri(int profileNumber, QString* outUrl)
{
    using ParameterMap = std::map<QString, QString>;
    ParameterMap params =
    {
        {kHanwhaChannelProperty, QString::number(m_hanwhaResource->getChannel())},
        {kHanwhaStreamingModeProperty, kHanwhaFullMode},
        {kHanwhaStreamingTypeProperty, kHanwhaRtpUnicast},
        {kHanwhaTransportProtocolProperty, rtpTransport()},
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
        if (profileNumber == kHanwhaInvalidProfile && !m_hanwhaResource->isNvr())
            profileNumber = 2; //< The actual number doesn't matter.
    }
    else
    {
        params.emplace(kHanwhaMediaTypeProperty, kHanwhaLiveMediaType);
    }

    if (m_hanwhaResource->isNvr())
        params.emplace(kHanwhaClientTypeProperty, "PC");

    if (profileNumber != kHanwhaInvalidProfile)
        params.emplace(kHanwhaProfileNumberProperty, QString::number(profileNumber));

    HanwhaRequestHelper helper(m_hanwhaResource->sharedContext());
    helper.setIgnoreMutexAnalyzer(true);
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
        qDebug() << "============ GOT PLAYBACK URL!!! (EDGE RECORDING)" << *outUrl
            << "Time Range:"
            << QDateTime::fromMSecsSinceEpoch(m_startTimeUsec / 1000)
            << QDateTime::fromMSecsSinceEpoch(m_endTimeUsec / 1000);
    }

    return CameraDiagnostics::NoErrorResult();
}

QString HanwhaStreamReader::rtpTransport() const
{
    QString transportStr = m_hanwhaResource
        ->getProperty(QnMediaResource::rtpTransportKey())
            .toUpper()
            .trimmed();

    if (transportStr == RtpTransport::udp)
        return kHanwhaUdp;

    return kHanwhaTcp;
}

void HanwhaStreamReader::setPositionUsec(qint64 value)
{
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

QnRtspClient& HanwhaStreamReader::rtspClient()
{
    return m_rtpReader.rtspClient();
}

QString HanwhaStreamReader::toHanwhaPlaybackTime(int64_t timestampUsec) const
{
    const auto timezoneShift = m_hanwhaResource
        ->sharedContext()
        ->timeZoneShift();

    return toHanwhaDateTime(timestampUsec / 1000, timezoneShift)
        .toString(lit("yyyyMMddhhmmss"));
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

    if (!m_timeSinceLastFrame.isValid())
        return QnAbstractMediaDataPtr();

    const auto context = m_hanwhaResource->sharedContext();
    const int speed = m_rtpReader.rtspClient().getScale();
    qint64 currentTimeMs = m_lastTimestampUsec / 1000 + m_timeSinceLastFrame.elapsedMs() * speed;
    const bool isForwardSearch = speed >= 0;
    const auto timeline = context->overlappedTimeline(m_hanwhaResource->getChannel());
    NX_ASSERT(timeline.size() <= 1, lit("There should be only one overlapped ID for NVRs"));
    if (timeline.size() != 1)
        return QnAbstractMediaDataPtr();

    const auto chunks = timeline.cbegin()->second;
    if (chunks.containTime(currentTimeMs))
    {
        if (m_timeSinceLastFrame.elapsed() < kTimeoutToExtrapolateTimeMs)
            return QnAbstractMediaDataPtr(); //< Don't forecast position too fast.
    }
    else
    {
        auto itr = chunks.findNearestPeriod(currentTimeMs, isForwardSearch);
        if (itr == chunks.end())
            currentTimeMs = isForwardSearch ? DATETIME_NOW : 0;
        else
            currentTimeMs = isForwardSearch ? itr->startTimeMs : itr->endTimeMs();
    }

    QnAbstractMediaDataPtr rez(new QnEmptyMediaData());
    rez->timestamp = currentTimeMs * 1000;
    if (speed < 0)
        rez->flags |= QnAbstractMediaData::MediaFlags_Reverse;
    return rez;
}

QnAbstractMediaDataPtr HanwhaStreamReader::getNextData()
{
    auto result = base_type::getNextData();
    if (result && result->dataType != QnAbstractMediaData::EMPTY_DATA)
    {
        m_lastTimestampUsec = result->timestamp;
        m_timeSinceLastFrame.restart();
    }

    return result;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
