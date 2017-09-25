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
    QString streamUrl;
    int nxProfileNumber = kHanwhaInvalidProfile;
    if (!m_hanwhaResource->isNvr())
    {
        nxProfileNumber = m_hanwhaResource->profileByRole(role);
        if (nxProfileNumber == kHanwhaInvalidProfile)
        {
            return CameraDiagnostics::CameraInvalidParams(
                lit("No profile for %1 stream").arg(
                    role == Qn::ConnectionRole::CR_LiveVideo
                    ? lit("primary")
                    : lit("secondary")));
        }

        if (isCameraControlRequired)
        {
            auto result = updateProfile(nxProfileNumber, params);
            if (!result)
                return result;
        }
    }
    auto result = streamUri(nxProfileNumber, &streamUrl);
    if (!result)
        return result;

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
    const auto codecProfile = m_hanwhaResource->streamCodecProfile(codec, role);
    const auto resolution = m_hanwhaResource->streamResolution(role);
    const auto frameRate = m_hanwhaResource->streamFrameRate(role, parameters.fps);
    const auto govLength = m_hanwhaResource->streamGovLength(role);
    const auto bitrateControl = m_hanwhaResource->streamBitrateControl(role);
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
    if (profileNumber == kHanwhaInvalidProfile)
    {
        return CameraDiagnostics::CameraPluginErrorResult(
            lit("Update profile: invalid profile number is given"));
    }

    HanwhaRequestHelper helper(m_hanwhaResource);
    const auto profileParameters = makeProfileParameters(profileNumber, parameters);

    m_hanwhaResource->beforeConfigureStream(getRole());
    const auto response = helper.update(lit("media/videoprofile"), profileParameters);
    m_hanwhaResource->afterConfigureStream(getRole());
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
    using ParameterMap = std::map<QString, QString>;
    ParameterMap params =
    {
        {kHanwhaChannelProperty, QString::number(m_hanwhaResource->getChannel())},
        {kHanwhaMediaTypeProperty, kHanwhaLiveMediaType},
        {kHanwhaStreamingModeProperty, kHanwhaFullMode},
        {kHanwhaStreamingTypeProperty, kHanwhaRtpUnicast},
        {kHanwhaTransportProtocolProperty, rtpTransport()},
        {kHanwhaRtspOverHttpProperty, kHanwhaFalse}
    };

    if (m_hanwhaResource->isNvr())
        params.emplace(kHanwhaClientTypeProperty, "PC");
    else
        params.emplace(kHanwhaProfileNumberProperty, QString::number(profileNumber));

    HanwhaRequestHelper helper(m_hanwhaResource);
    const auto response = helper.view(lit("media/streamuri"), params);

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
