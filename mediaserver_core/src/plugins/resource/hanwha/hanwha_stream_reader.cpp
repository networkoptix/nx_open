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
    int nxProfileNumber = m_hanwhaResource->profileByRole(role);
    if (nxProfileNumber == kHanwhaInvalidProfile)
    {
        return CameraDiagnostics::CameraInvalidParams(
            lit("No profile for %1 stream").arg(
                role == Qn::ConnectionRole::CR_LiveVideo
                    ? lit("primary")
                    : lit("secondary")));
    }

    auto result = updateProfile(nxProfileNumber, params);
    if (!result)
        return result;

    QString streamUrl;
    result = streamUri(nxProfileNumber, &streamUrl);

    m_rtpReader.setRole(role);
    m_rtpReader.setRequest(streamUrl);
    m_hanwhaResource->updateSourceUrl(streamUrl, role);

    return m_rtpReader.openStream();
}

HanwhaProfileParameters HanwhaStreamReader::makeProfileParameters(
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

    HanwhaProfileParameters result = {
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
        return error(
            response,
            CameraDiagnostics::RequestFailedResult(
                lit("media/videoprofile/update"),
                response.errorString()));
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
        return error(
            response,
            CameraDiagnostics::RequestFailedResult(
                lit("media/streamuri/view"),
                response.errorString()));
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
