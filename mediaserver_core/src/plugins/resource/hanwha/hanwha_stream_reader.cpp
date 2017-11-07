#if defined(ENABLE_HANWHA)

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

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

static const QString kLive4NvrProfileName = lit("Live4NVR");
static const int kHanwhaDefaultPrimaryStreamProfile = 2;

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
    if (!m_hanwhaResource->isNvr())
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

    if (m_hanwhaResource->isNvr())
        m_rtpReader.setTimePolicy(TimePolicy::ForceCameraTime);
    else if (role == Qn::ConnectionRole::CR_Archive)
        m_rtpReader.setTimePolicy(TimePolicy::OnvifExtension);
    else
        m_rtpReader.setTimePolicy(TimePolicy::IgnoreCameraTimeIfBigJitter);

    if (!m_rateControlEnabled)
        m_rtpReader.addRequestHeader(lit("PLAY"), nx_http::HttpHeader("Rate-Control", "no"));

    m_rtpReader.setRequest(streamUrlString);
    m_hanwhaResource->updateSourceUrl(streamUrlString, role);

    return m_rtpReader.openStream();
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
    if (m_hanwhaResource->isNvr())
    {
        // TODO: #dmishin implement profile configuration for NVR if needed.
        return CameraDiagnostics::NoErrorResult();
    }

    if (profileNumber == kHanwhaInvalidProfile)
    {
        return CameraDiagnostics::CameraPluginErrorResult(
            lit("Update profile: invalid profile number is given"));
    }

    HanwhaRequestHelper helper(m_hanwhaResource->sharedContext());
    helper.setIgnoreMutexAnalyzer(true);
    const auto profileParameters = makeProfileParameters(profileNumber, parameters);

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
        || m_hanwhaResource->isNvr()
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

        params.emplace(kHanwhaMediaTypeProperty, mediaType);
    }
    else
    {
        params.emplace(kHanwhaMediaTypeProperty, kHanwhaLiveMediaType);
    }

    if (m_hanwhaResource->isNvr())
        params.emplace(kHanwhaClientTypeProperty, "PC");

    if (profileNumber == kHanwhaInvalidProfile
        && !m_hanwhaResource->isNvr()
        && role == Qn::ConnectionRole::CR_Archive)
    {
        profileNumber = 2; //< The actual number doesn't matter.
    }

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

    if (getRole() == Qn::CR_Archive && !m_hanwhaResource->isNvr())
    {
        QUrl url(*outUrl);

        // This path is not documented, but Samsung SmartViewer uses it.
        // May not work with some cameras.
        url.setPath(lit("/recording/%1-%2/OverlappedID=%3/play.smp")
            .arg(toHanwhaPlaybackTime(m_startTimeUsec))
            .arg(toHanwhaPlaybackTime(m_endTimeUsec))
            .arg(m_overlappedId));

        *outUrl = url.toString();

        qDebug() << "============ GOT PLAYBACK URL!!! (EDGE RECORDING)" << *outUrl;
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

void HanwhaStreamReader::setOverlappedId(int overlappedId)
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

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
