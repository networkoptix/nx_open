#if defined(ENABLE_ONVIF)

#include "hikvision_hevc_stream_reader.h"

#include <algorithm>

#include <QtXml/QDomDocument>

#include <nx/network/http/http_client.h>
#include <plugins/resource/hikvision/hikvision_resource.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

using namespace nx::mediaserver_core::plugins::hikvision;

HikvisionHevcStreamReader::HikvisionHevcStreamReader(const QnResourcePtr& resource):
    QnRtpStreamReader(resource)
{
    m_hikvisionResource = resource.dynamicCast<HikvisionResource>();
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

    auto streamingUrl = buildHikvisionStreamUrl(channelProperties.rtspPortNumber);
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
    auto resolution = chooseResolution(channelCapabilities, m_hikvisionResource->getPrimaryResolution());
    auto codec = chooseCodec(channelCapabilities);
    auto fps = chooseFps(channelCapabilities, liveStreamParameters.fps);

    boost::optional<int> quality = boost::none;
    if (getRole() == Qn::ConnectionRole::CR_LiveVideo)
        quality = chooseQuality(liveStreamParameters.quality, channelCapabilities);
    else
        quality = chooseQuality(liveStreamParameters.secondaryQuality, channelCapabilities);

    result = configureChannel(resolution, codec, fps, quality);
    if (!result)
        return result;

    m_rtpReader.setRole(getRole());
    m_rtpReader.setRequest(streamingUrl.toString());
    return m_rtpReader.openStream();
}

utils::Url HikvisionHevcStreamReader::buildHikvisionStreamUrl(int rtspPortNumber) const
{
    auto url = nx::utils::Url(m_hikvisionResource->getUrl());
    url.setScheme(lit("rtsp"));
    url.setPort(rtspPortNumber);
    url.setPath(kChannelStreamingPathTemplate.arg(
        buildChannelNumber(getRole(), m_hikvisionResource->getChannel())));

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

    auto secondaryResolution = QnPhysicalCameraResource::getNearestResolution(
        SECONDARY_STREAM_DEFAULT_RESOLUTION,
        QnPhysicalCameraResource::getResolutionAspectRatio(primaryResolution),
        maxArea,
        QList<QSize>::fromVector(QVector<QSize>::fromStdVector(resolutions)));

    if (secondaryResolution.isEmpty())
    {
        secondaryResolution = QnPhysicalCameraResource::getNearestResolution(
            SECONDARY_STREAM_DEFAULT_RESOLUTION,
            0.0,
            maxArea,
            QList<QSize>::fromVector(QVector<QSize>::fromStdVector(resolutions)));
    }

    return secondaryResolution;
}

QString HikvisionHevcStreamReader::chooseCodec(
    const ChannelCapabilities& channelCapabilities) const
{
    // TODO: #dmishin use constants instead of literals 
    if (codecSupported(AV_CODEC_ID_HEVC, channelCapabilities))
        return lit("H.265");
    else if (codecSupported(AV_CODEC_ID_H264, channelCapabilities))
        return lit("H.264");
    else if (codecSupported(AV_CODEC_ID_MJPEG, channelCapabilities))
        return lit("MJPEG");

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
    if (quality > Qn::StreamQuality::QualityHighest)
        return boost::none;

    return rescaleQuality(
        channelCapabilities.quality,
        kStreamQualityCount,
        (int)quality);
}

boost::optional<int> HikvisionHevcStreamReader::chooseQuality(
    Qn::SecondStreamQuality quality,
    const ChannelCapabilities& channelCapabilities) const
{
    const int kSecondStreamQualityCount = 3;
    if (quality > Qn::SecondStreamQuality::SSQualityHigh)
        return boost::none;

    return rescaleQuality(
        channelCapabilities.quality,
        kSecondStreamQualityCount,
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
    auto outputIndex = (int)std::round(outputQuality.size() / inputScale) - 1;

    bool indexIsCorrect = outputIndex < outputQuality.size() && outputIndex >= 0; 
    NX_ASSERT(indexIsCorrect, lit("Wrong Hikvision quality index."));
    if (!indexIsCorrect)
        return boost::none;

    return outputQuality[outputIndex];
}

CameraDiagnostics::Result HikvisionHevcStreamReader::fetchChannelProperties(
    ChannelProperties* outChannelProperties) const
{
    auto url = hikvisionRequestUrlFromPath(kChannelStreamingPathTemplate.arg(
        buildChannelNumber(getRole(), m_hikvisionResource->getChannel())));

    nx::Buffer response;
    nx_http::StatusCode::Value statusCode;
    if (!doGetRequest(url, m_hikvisionResource->getAuth(), &response, &statusCode))
    {
        if (statusCode == nx_http::StatusCode::Value::unauthorized)
            return CameraDiagnostics::NotAuthorisedResult(url.toString());

        return CameraDiagnostics::RequestFailedResult(
            lit("Fetch channel properties"),
            lit("Request failed"));
    }

    if (!parseChannelPropertiesResponse(response, outChannelProperties))
    {
        return CameraDiagnostics::CameraResponseParseErrorResult(
            url.toString(),
            lit("Fetch channel properties"));
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HikvisionHevcStreamReader::configureChannel(
    QSize resolution,
    QString codec,
    int fps,
    boost::optional<int> quality)
{
    auto url = hikvisionRequestUrlFromPath(kChannelStreamingPathTemplate.arg(
        buildChannelNumber(getRole(), m_hikvisionResource->getChannel())));

    nx::Buffer responseBuffer;
    nx_http::StatusCode::Value statusCode;
    bool result = doGetRequest(url, m_hikvisionResource->getAuth(), &responseBuffer, &statusCode);

    if (!result)
    {
        if (statusCode == nx_http::StatusCode::Value::unauthorized)
            return CameraDiagnostics::NotAuthorisedResult(url.toString());

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
        quality);

    if (!result)
    {
        return CameraDiagnostics::CameraResponseParseErrorResult(
            url.toString(),
            responseBuffer);
    }

    result = doPutRequest(
        url,
        m_hikvisionResource->getAuth(),
        videoChannelConfiguration.toString().toUtf8(),
        &statusCode);

    if (!result)
    {
        if (statusCode == nx_http::StatusCode::Value::unauthorized)
            return CameraDiagnostics::NotAuthorisedResult(url.toString());

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
    boost::optional<int> quality) const
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

    bool elementsAreOk = !resolutionWidthElement.isNull()
        && !resolutionHeightElement.isNull()
        && !codecElement.isNull()
        && !fpsElement.isNull()
        && !qualityElement.isNull();

    if (!elementsAreOk)
        return false;

    resolutionWidthElement.firstChild().setNodeValue(QString::number(resolution.width()));
    resolutionHeightElement.firstChild().setNodeValue(QString::number(resolution.height()));
    codecElement.firstChild().setNodeValue(codec);
    fpsElement.firstChild().setNodeValue(QString::number(fps));
    if (quality)
        qualityElement.firstChild().setNodeValue(QString::number(quality.get()));

    return true;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // ENABLE_ONVIF
