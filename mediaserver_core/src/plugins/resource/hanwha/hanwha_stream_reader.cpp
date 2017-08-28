#if defined(ENABLE_HANWHA)

#include "hanwha_stream_reader.h"
#include "hanwha_resource.h"
#include "hanwha_request_helper.h"
#include "hanwha_utils.h"
#include "hanwha_common.h"

#include <nx/utils/scope_guard.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

const QString kPrimaryNxProfileName = lit("NxPrimary");
const QString kSecondaryNxProfileName = lit("NxSecondary");

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
    int nxProfileNumber = kHanwhaInvalidProfile;
    int profileToRemove = kHanwhaInvalidProfile;

    CameraDiagnostics::Result result = CameraDiagnostics::UnknownErrorResult();

    {
        // TODO: #dmishin rename before(after)ConfigureStream
        // and move it to QnSecurityCamResource
        m_hanwhaResource->beforeConfigureStream(getRole());
        auto guard = makeScopeGuard([this](){m_hanwhaResource->afterConfigureStream(getRole());});

        result = findProfile(&nxProfileNumber, &profileToRemove);
        if (!result)
            return result;

        if (profileToRemove != kHanwhaInvalidProfile)
            result = removeProfile(profileToRemove);
    }

    if (nxProfileNumber == kHanwhaInvalidProfile)
        result = createProfile(&nxProfileNumber);

    if (!result)
        return result;

    result = updateProfile(nxProfileNumber, params);

    if (!result)
        return result;

    QString streamUrl;
    result = streamUri(nxProfileNumber, &streamUrl);

    const auto role = getRole();

    m_rtpReader.setRole(role);
    m_rtpReader.setRequest(streamUrl);
    m_hanwhaResource->updateSourceUrl(streamUrl, role);
    m_hanwhaResource->setProfileForRole(role, nxProfileNumber);

    return m_rtpReader.openStream();
}

HanwhaStreamReader::ProfileMap HanwhaStreamReader::parseProfiles(
    const HanwhaResponse& response) const
{
    NX_ASSERT(response.isSuccessful());
    if (!response.isSuccessful())
        return ProfileMap();

    ProfileMap profiles;
    const auto channel = m_hanwhaResource->getChannel();
    for (const auto& entry: response.response())
    {
        const auto split = entry.first.split(L'.');
        const auto splitSize = split.size();

        if (splitSize < 5)
            continue;

        if (split[0] != kHanwhaChannelProperty)
            continue;

        bool success = false;
        const auto profileChannel = split[1].toInt(&success);
        if (!success || profileChannel != channel)
            continue;

        if (split[2] != kHanwhaProfileNumberProperty)
            continue;

        const auto profileNumber = split[3].toInt(&success);
        if (!success)
            continue;

        profiles[profileNumber].setParameter(split.mid(4).join(L'.'), entry.second);
    }

    for (auto& entry: profiles)
        entry.second.number = entry.first;

    return profiles;
}

QString HanwhaStreamReader::nxProfileName() const
{
    const auto role = getRole();
    switch (role)
    {
        case Qn::ConnectionRole::CR_LiveVideo:
            return kPrimaryNxProfileName;
        case Qn::ConnectionRole::CR_SecondaryLiveVideo:
            return kSecondaryNxProfileName;
        default:
            NX_ASSERT(false, "Wrong role");
            return QString();
    }
}

bool HanwhaStreamReader::isNxProfile(const QString& profileName) const
{
    return profileName == kPrimaryNxProfileName
        || profileName == kSecondaryNxProfileName;
}

HanwhaStreamReader::ProfileParameters HanwhaStreamReader::makeProfileParameters(
    int profileNumber,
    const QnLiveStreamParams& parameters) const
{
    const auto role = getRole();
    const auto codec = m_hanwhaResource->streamCodec(role);
    const auto resolution = m_hanwhaResource->streamResolution(role);
    const auto frameRate = m_hanwhaResource->closestFrameRate(role, parameters.fps);
    const auto govLength = m_hanwhaResource->streamGovLength(role);

    const auto govLengthParameterName = 
        lit("%1.GOVLength").arg(toHanwhaString(codec));

    const bool govLengthParameterIsNeeded = 
        (codec == AVCodecID::AV_CODEC_ID_H264
        || codec == AVCodecID::AV_CODEC_ID_HEVC)
            && govLength != kHanwhaInvalidGovLength;

    ProfileParameters result = {
        {kHanwhaChannelProperty, QString::number(m_hanwhaResource->getChannel())},
        {kHanwhaProfileNumberProperty, QString::number(profileNumber)},
        {kHanwhaEncodingTypeProperty, toHanwhaString(codec)},
        {kHanwhaFrameRateProperty, QString::number(
            m_hanwhaResource->closestFrameRate(role, frameRate))},
        {kHanwhaResolutionProperty, toHanwhaString(resolution)},
        {kHanwhaAudioInputEnableProperty, toHanwhaString(m_hanwhaResource->isAudioEnabled())}
    };

    if (govLengthParameterIsNeeded)
        result.emplace(govLengthParameterName, QString::number(govLength));

    return result;
}

CameraDiagnostics::Result HanwhaStreamReader::findProfile(
    int* outProfileNumber,
    int* profileToRemoveIfProfilesExhausted)
{
    NX_ASSERT(outProfileNumber);
    if (!outProfileNumber)
    {
        return CameraDiagnostics::CameraPluginErrorResult(
            lit("Find profile: no output profile number provided"));
    }

    NX_ASSERT(profileToRemoveIfProfilesExhausted);
    if (!profileToRemoveIfProfilesExhausted)
    {
        return CameraDiagnostics::CameraPluginErrorResult(
            lit("Find profile: no profile to remove"));
    }

    *profileToRemoveIfProfilesExhausted = kHanwhaInvalidProfile;
    *outProfileNumber = kHanwhaInvalidProfile;

    HanwhaRequestHelper helper(m_hanwhaResource);
    const auto response = helper.view(lit("media/videoprofile"));

    if (!response.isSuccessful())
    {
        return CameraDiagnostics::RequestFailedResult(
            lit("media/videoprofile/view"),
            response.errorString());
    }

    const auto availableProfiles = parseProfiles(response);
    const auto predefinedProfileName = nxProfileName();
    const bool needToRemoveProfile = 
        availableProfiles.size() >= m_hanwhaResource->maxProfileCount();

    for (const auto& entry: availableProfiles)
    {
        const auto& profile = entry.second;
        if (profile.name == predefinedProfileName)
        {
            *outProfileNumber = profile.number;
            return CameraDiagnostics::NoErrorResult();
        }

        if (needToRemoveProfile)
        {
            bool canRemoveProfile = !profile.fixed
                && !isNxProfile(profile.name)
                && *outProfileNumber == kHanwhaInvalidProfile;

            if (canRemoveProfile)
                *profileToRemoveIfProfilesExhausted = profile.number;
        }
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaStreamReader::createProfile(
    int* outProfileNumber)
{
    NX_ASSERT(outProfileNumber);
    if (!outProfileNumber)
    {
        return CameraDiagnostics::CameraPluginErrorResult(
            lit("Create profile: output profile number is null"));
    }

    HanwhaRequestHelper helper(m_hanwhaResource);
    const auto response = helper.add(
        lit("media/videoprofile"),
        {
            {kHanwhaProfileNameProperty, nxProfileName()},
            {kHanwhaEncodingTypeProperty, toHanwhaString(
                m_hanwhaResource->streamCodec(getRole()))}
        });

    if (!response.isSuccessful())
    {
        return CameraDiagnostics::RequestFailedResult(
            lit("media/videoprofile/add"),
            response.errorString());
    }

    bool success = false;
    *outProfileNumber = response.response()[kHanwhaProfileNumberProperty].toInt(&success);

    if (!success)
    {
        return CameraDiagnostics::CameraInvalidParams(
            lit("Invalid profile number string"));
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaStreamReader::updateProfile(
    int profileNumber,
    const QnLiveStreamParams& parameters)
{
    if (profileNumber == kHanwhaInvalidProfile)
    {
        return CameraDiagnostics::CameraPluginErrorResult(
            lit("Update profile: invalid profile number is given"));
    }

    HanwhaRequestHelper helper(m_hanwhaResource);
    const auto profileParameters = makeProfileParameters(profileNumber, parameters);
    const auto response = helper.update(lit("media/videoprofile"), profileParameters);
    if (!response.isSuccessful())
    {
        return CameraDiagnostics::RequestFailedResult(
            lit("media/videoprofile/update"),
            response.errorString());
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaStreamReader::removeProfile(int profileNumber)
{
    HanwhaRequestHelper helper(m_hanwhaResource);
    const auto response = helper.remove(
        lit("media/videoprofile"),
        {
            {kHanwhaChannelProperty, QString::number(m_hanwhaResource->getChannel())},
            {kHanwhaProfileNameProperty, QString::number(profileNumber)}
        });

    if (!response.isSuccessful())
    {
        return CameraDiagnostics::RequestFailedResult(
            lit("media/videoprofile/remove"),
            response.errorString());
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HanwhaStreamReader::streamUri(int profileNumber, QString* outUrl)
{
    if (profileNumber == kHanwhaInvalidProfile)
    {
        return CameraDiagnostics::CameraPluginErrorResult(
            lit("Get stream URI: invalid profile number is given"));
    }

    HanwhaRequestHelper helper(m_hanwhaResource);
    const auto response = helper.view(
        lit("media/streamuri"),
        {
            {kHanwhaChannelProperty, QString::number(m_hanwhaResource->getChannel())},
            {kHanwhaProfileNumberProperty, QString::number(profileNumber)},
            {kHanwhaMediaTypeProperty, kHanwhaLiveMediaType},
            {kHanwhaStreamingModeProperty, kHanwhaFullMode},
            {kHanwhaStreamingTypeProperty, kHanwhaRtpUnicast},
            {kHanwhaTransportProtocolProperty, rtpTransport()},
            {kHanwhaRtspOverHttpProperty, kHanwhaFalse}
        });

    if (!response.isSuccessful())
    {
        return CameraDiagnostics::RequestFailedResult(
            lit("media/streamuri/view"),
            response.errorString());
    }

    *outUrl = response.response()[kHanwhaUriProperty];
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

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
