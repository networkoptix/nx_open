#if defined(ENABLE_ONVIF)

#include "hikvision_hevc_stream_reader.h"

#include <algorithm>

#include <QtXml/QDomDocument>

#include <nx/network/http/http_client.h>
#include <nx/network/rtsp/rtsp_types.h>
#include <plugins/resource/hikvision/hikvision_resource.h>
#include <utils/media/av_codec_helper.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

using namespace nx::mediaserver_core::plugins::hikvision;

HikvisionHevcStreamReader::HikvisionHevcStreamReader(const HikvisionResourcePtr& resource):
    QnRtpStreamReader(resource),
    m_hikvisionResource(resource)
{
}

CameraDiagnostics::Result HikvisionHevcStreamReader::openStreamInternal(
    bool isCameraControlRequired,
    const QnLiveStreamParams& liveStreamParameters)
{
    ChannelProperties channelProperties;
    auto result = fetchChannelProperties(&channelProperties);
    if (!result)
        return result;

    auto role = getRole();
    if (role == Qn::CR_LiveVideo &&
        !m_hikvisionResource->getPtzConfigurationToken().isEmpty() &&
        m_hikvisionResource->getPtzProfileToken().isEmpty())
    {
        // Need to assign some Onvif profile to execute PTZ commands
        m_hikvisionResource->findDefaultPtzProfileToken();
    }

    auto streamingUrl = buildHikvisionStreamUrl(channelProperties);
    m_hikvisionResource->updateSourceUrl(streamingUrl.toString(), getRole());
    if (!isCameraControlRequired)
    {
        m_rtpReader.setRole(role);
        m_rtpReader.setRequest(streamingUrl.toString());
        return m_rtpReader.openStream();
    }

    auto optionalChannelCapabilities = m_hikvisionResource->channelCapabilities(role);
    if (!optionalChannelCapabilities)
    {
        return CameraDiagnostics::CameraInvalidParams(
            lm("No channel capabilities for role %1")
                .arg(role == Qn::ConnectionRole::CR_LiveVideo ? "primary" : "secondary"));
    }

    auto channelCapabilities = optionalChannelCapabilities.get();
    auto resolutionList = m_hikvisionResource->primaryVideoCapabilities().resolutions;
    QSize resolution = liveStreamParameters.resolution;
    if (resolution.isEmpty())
        resolution = chooseResolution(channelCapabilities, liveStreamParameters.resolution);
    auto codec = chooseCodec(
        channelCapabilities,
        QnAvCodecHelper::codecIdFromString(liveStreamParameters.codec));
    auto fps = chooseFps(channelCapabilities, liveStreamParameters.fps);

    boost::optional<int> quality = boost::none;
    quality = chooseQuality(liveStreamParameters.quality, channelCapabilities);

    result = configureChannel(channelProperties, resolution, codec, fps, quality, liveStreamParameters.bitrateKbps);
    if (!result)
        return result;

    m_rtpReader.setRole(getRole());
    m_rtpReader.setRequest(streamingUrl.toString());
    return m_rtpReader.openStream();
}

nx::utils::Url HikvisionHevcStreamReader::buildHikvisionStreamUrl(
    const hikvision::ChannelProperties& properties) const
{
    auto url = properties.httpUrl;
    url.setScheme(QString::fromUtf8(nx_rtsp::kUrlSchemeName));
    url.setPort(properties.rtspPort);
    url.setQuery(QString());
    return url;
}

nx::utils::Url HikvisionHevcStreamReader::hikvisionRequestUrlFromPath(const QString& path) const
{
    auto url = nx::utils::Url(m_hikvisionResource->getUrl());
    url.setPath(path);

    return url;
}

QSize HikvisionHevcStreamReader::chooseResolution(
    const ChannelCapabilities& channelCapabilities,
    const QSize& primaryResolution) const
{
    auto& resolutions = channelCapabilities.resolutions;
    NX_ASSERT(!resolutions.empty(), lit("Resolution list is empty."));
    if (resolutions.empty())
        return QSize();

    if (getRole() == Qn::ConnectionRole::CR_LiveVideo)
        return primaryResolution;

    const int maxArea = SECONDARY_STREAM_MAX_RESOLUTION.width()
        * SECONDARY_STREAM_MAX_RESOLUTION.height();

    auto secondaryResolution = nx::mediaserver::resource::Camera::getNearestResolution(
        SECONDARY_STREAM_DEFAULT_RESOLUTION,
        nx::mediaserver::resource::Camera::getResolutionAspectRatio(primaryResolution),
        maxArea,
        QList<QSize>::fromVector(QVector<QSize>::fromStdVector(resolutions)));

    if (secondaryResolution.isEmpty())
    {
        secondaryResolution = nx::mediaserver::resource::Camera::getNearestResolution(
            SECONDARY_STREAM_DEFAULT_RESOLUTION,
            0.0,
            maxArea,
            QList<QSize>::fromVector(QVector<QSize>::fromStdVector(resolutions)));
    }

    return secondaryResolution;
}

QString codecToHikvisionString(AVCodecID codec)
{
    switch (codec)
    {
    case AV_CODEC_ID_HEVC:
        return lit("H.265");
    case AV_CODEC_ID_H264:
        return lit("H.264");
    case AV_CODEC_ID_MJPEG:
        return lit("MJPEG");
    default:
        NX_ASSERT(0, "Unsupported codec");
        return QString();
    }
}

QString HikvisionHevcStreamReader::chooseCodec(
    const ChannelCapabilities& channelCapabilities,
    AVCodecID codec) const
{
    if (codecSupported(codec, channelCapabilities))
        return codecToHikvisionString(codec);
    else if (codecSupported(AV_CODEC_ID_HEVC, channelCapabilities))
        return codecToHikvisionString(AV_CODEC_ID_HEVC);
    else if (codecSupported(AV_CODEC_ID_H264, channelCapabilities))
        return codecToHikvisionString(AV_CODEC_ID_H264);
    else if (codecSupported(AV_CODEC_ID_MJPEG, channelCapabilities))
        return codecToHikvisionString(AV_CODEC_ID_MJPEG);

    return QString();
}

int HikvisionHevcStreamReader::chooseFps(
    const ChannelCapabilities& channelCapabilities, float fps) const
{
    int choosenFps = 0;
    auto minDifference = std::numeric_limits<double>::max();

    for (const auto& hikvisionFramerate: channelCapabilities.fps)
    {
        auto difference = std::abs(hikvisionFramerate / 100.0f - fps);

        if (difference < minDifference)
        {
            minDifference = difference;
            choosenFps = hikvisionFramerate;
        }
    }

    return choosenFps;
}

boost::optional<int> HikvisionHevcStreamReader::chooseQuality(
    Qn::StreamQuality quality,
    const ChannelCapabilities& channelCapabilities) const
{
    const int kStreamQualityCount = 5;
    if (quality > Qn::StreamQuality::highest)
        return boost::none;

    return rescaleQuality(
        channelCapabilities.quality,
        kStreamQualityCount,
        (int)quality);
}

boost::optional<int> HikvisionHevcStreamReader::rescaleQuality(
    const std::vector<int>& outputQuality,
    int inputScaleSize,
    int inputQualityIndex) const
{
    bool parametersAreOk = inputScaleSize > 0
        && outputQuality.size() > 0
        && inputQualityIndex < inputScaleSize
        && inputQualityIndex >= 0;

    NX_ASSERT(parametersAreOk, lm("Rescaling quality, parameters are incorrect."));
    if (!parametersAreOk)
        return boost::none;

    auto outputScaleSize = outputQuality.size();
    auto inputScale = (double)inputScaleSize / (inputQualityIndex + 1);
    auto outputIndex = qRound(outputQuality.size() / inputScale) - 1;

    bool indexIsCorrect = outputIndex < outputQuality.size() && outputIndex >= 0;
    NX_ASSERT(indexIsCorrect, lit("Wrong Hikvision quality index."));
    if (!indexIsCorrect)
        return boost::none;

    return outputQuality[outputIndex];
}

CameraDiagnostics::Result HikvisionHevcStreamReader::fetchChannelProperties(
    ChannelProperties* outChannelProperties) const
{
    const auto kRequestName = lit("Fetch channel properties");
    nx_http::StatusCode::Value statusCode = nx_http::StatusCode::undefined;
    for (const auto& path: {kChannelStreamingPathTemplate, kChannelStreamingPathForNvrTemplate})
    {
        auto url = hikvisionRequestUrlFromPath(path.arg(
            buildChannelNumber(getRole(), m_hikvisionResource->getChannel())));

        nx::Buffer response;
        if (!doGetRequest(url, m_hikvisionResource->getAuth(), &response, &statusCode))
        {
            if (statusCode == nx::network::http::StatusCode::Value::unauthorized)
                return CameraDiagnostics::NotAuthorisedResult(url.toString());

            if (statusCode != nx::network::http::StatusCode::Value::ok)
                continue;
        }

        if (!parseChannelPropertiesResponse(response, outChannelProperties))
            return CameraDiagnostics::CameraResponseParseErrorResult(url.toString(), kRequestName);

        outChannelProperties->httpUrl = url;
        return CameraDiagnostics::NoErrorResult();
    }
    if (statusCode != nx::network::http::StatusCode::Value::ok)
        return CameraDiagnostics::RequestFailedResult(kRequestName, toString(statusCode));

    return CameraDiagnostics::RequestFailedResult(kRequestName, lit("No sutable URL"));
}

CameraDiagnostics::Result HikvisionHevcStreamReader::configureChannel(
    const hikvision::ChannelProperties& channelProperties,
    QSize resolution,
    QString codec,
    int fps,
    boost::optional<int> quality,
    boost::optional<int> bitrateKbps)
{
    nx::Buffer responseBuffer;
    nx::network::http::StatusCode::Value statusCode;
    bool result = doGetRequest(
        channelProperties.httpUrl, m_hikvisionResource->getAuth(),
        &responseBuffer, &statusCode);

    if (!result)
    {
        if (statusCode == nx::network::http::StatusCode::Value::unauthorized)
            return CameraDiagnostics::NotAuthorisedResult(channelProperties.httpUrl.toString());

        return CameraDiagnostics::RequestFailedResult(
            lit("Fetch video channel configuration."),
            lit("Request failed."));
    }

    QDomDocument videoChannelConfiguration;
    videoChannelConfiguration.setContent(responseBuffer);

    result = updateVideoChannelConfiguration(
        &videoChannelConfiguration,
        resolution,
        codec,
        fps,
        quality,
        bitrateKbps);

    if (!result)
    {
        return CameraDiagnostics::CameraResponseParseErrorResult(
            channelProperties.httpUrl.toString(),
            responseBuffer);
    }
    // Workaround camera bug. It wont change bitrate if codec just changed.
    const bool execTwice = m_previousCodecValue != codec;
    m_previousCodecValue = codec;

    NX_VERBOSE(this, lm("video configuration for resource %1: %2")
        .args(m_resource->getUniqueId(), videoChannelConfiguration.toString().toUtf8()));
    result = doPutRequest(
        channelProperties.httpUrl,
        m_hikvisionResource->getAuth(),
        videoChannelConfiguration.toString().toUtf8(),
        &statusCode);
    if (result && execTwice)
    {
        result = doPutRequest(
            channelProperties.httpUrl,
            m_hikvisionResource->getAuth(),
            videoChannelConfiguration.toString().toUtf8(),
            &statusCode);
    }
    if (!result)
    {
        if (statusCode == nx::network::http::StatusCode::Value::unauthorized)
            return CameraDiagnostics::NotAuthorisedResult(channelProperties.httpUrl.toString());

        return CameraDiagnostics::RequestFailedResult(
            lit("Update video channel configuration."),
            lit("Request failed."));
    }

    return CameraDiagnostics::NoErrorResult();
}

bool HikvisionHevcStreamReader::updateVideoChannelConfiguration(
    QDomDocument* outVideoChannelConfiguration,
    const QSize& resolution,
    const QString& codec,
    int fps,
    boost::optional<int> quality,
    boost::optional<int> bitrateKbps) const
{
    auto element = outVideoChannelConfiguration->documentElement();
    if (element.isNull() || element.tagName() != kChannelRootElementTag)
        return false;

    auto videoElement = element.firstChildElement(kVideoElementTag);
    if (videoElement.isNull())
        return false;

    auto resolutionWidthElement = videoElement.firstChildElement(kVideoResolutionWidthTag);
    auto resolutionHeightElement = videoElement.firstChildElement(kVideoResolutionHeightTag);
    auto codecElement = videoElement.firstChildElement(kVideoCodecTypeTag);
    auto fpsElement = videoElement.firstChildElement(kMaxFrameRateTag);
    auto qualityElement = videoElement.firstChildElement(kFixedQualityTag);
    auto bitrateElement = videoElement.firstChildElement(kFixedBitrateTag);

    bool elementsAreOk = !resolutionWidthElement.isNull()
        && !resolutionHeightElement.isNull()
        && !codecElement.isNull()
        && !fpsElement.isNull()
        && !qualityElement.isNull()
        && !bitrateElement.isNull();

    if (!elementsAreOk)
        return false;

    resolutionWidthElement.firstChild().setNodeValue(QString::number(resolution.width()));
    resolutionHeightElement.firstChild().setNodeValue(QString::number(resolution.height()));
    codecElement.firstChild().setNodeValue(codec);
    fpsElement.firstChild().setNodeValue(QString::number(fps));
    if (quality)
        qualityElement.firstChild().setNodeValue(QString::number(quality.get()));
    if (bitrateKbps && *bitrateKbps > 0)
        bitrateElement.firstChild().setNodeValue(QString::number(*bitrateKbps));

    return true;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // ENABLE_ONVIF
