#if defined(ENABLE_ONVIF)

#include "vivotek_resource.h"

#include <chrono>
#include <type_traits>

#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

#include <common/common_module.h>
#include <core/resource_management/resource_data_pool.h>
#include <utils/media/av_codec_helper.h>

namespace nx {
namespace vms::server {
namespace plugins {

namespace {

const QString kGetParameterPath = lit("/cgi-bin/admin/getparam.cgi");
const QString kSetParameterPath = lit("/cgi-bin/admin/setparam.cgi");
const QString kInvalid = lit("ERR_INVALID");
const QString kHevcCodecString = lit("h265");

const QString kGeneralCodecCapability = lit("capability_videoin_codec");
// codec capability per stream
const QString kStreamCodecCapabilities = lit("capability_videoin_streamcodec");
const QString kStreamCodecParameterTemplate = lit("videoin_c%1_s%2_codectype");

const std::chrono::milliseconds kHttpTimeout(5000);

using StreamCodecCapabilityUnderlyingType =
    std::underlying_type<VivotekResource::StreamCodecCapability>::type;

} // namespace

QString VivotekResource::defaultCodec() const
{
    return QnAvCodecHelper::codecIdToString(AV_CODEC_ID_H265);
}

VivotekResource::VivotekResource(QnMediaServerModule* serverModule):
    QnPlOnvifResource(serverModule)
{
}

CameraDiagnostics::Result VivotekResource::initializeMedia(
    const _onvifDevice__GetCapabilitiesResponse& onvifCapabilities)
{
    auto result = base_type::initializeMedia(onvifCapabilities);
    if (!result)
        return result;

    bool hevcIsDisabled = resourceData().value<bool>(ResourceDataKey::kDisableHevc, false);

    if (hevcIsDisabled)
        return result;

    fetchHevcSupport();
    auto hevcIsSupported = hasHevcSupport();

    if (!hevcIsSupported || !hevcIsSupported.get())
        return result;

    auto rawStreamCodecCapabilities = getVivotekParameter(kStreamCodecCapabilities);
    if (!rawStreamCodecCapabilities)
    {
        return CameraDiagnostics::RequestFailedResult(
            kStreamCodecCapabilities,
            lit("Request failed"));
    }

    m_streamCodecCapabilities.clear();
    bool streamCodecCapabilitiesParsed = parseStreamCodecCapabilities(
        rawStreamCodecCapabilities.get(),
        &m_streamCodecCapabilities);

    if (!streamCodecCapabilitiesParsed)
    {
        return CameraDiagnostics::CameraResponseParseErrorResult(
            kStreamCodecCapabilities,
            lit("Stream codec capabilities"));
    }

    return result;
}

CameraDiagnostics::Result VivotekResource::customStreamConfiguration(
    Qn::ConnectionRole role)
{
    bool success = true;
    if (streamSupportsHevc(role))
        success = setHevcForStream(role);

    if (!success)
    {
        return CameraDiagnostics::RequestFailedResult(
            lit("Set HEVC for stream %1")
            .arg(role == Qn::ConnectionRole::CR_LiveVideo
                ? lit("primary")
                : lit("secondary")),
            lit("Request failed."));
    }

    return CameraDiagnostics::NoErrorResult();
}

bool VivotekResource::fetchHevcSupport()
{
    if (m_hasHevcSupport.is_initialized())
        return true;

    auto value = getVivotekParameter(kGeneralCodecCapability);
    if (!value)
    {
        m_hasHevcSupport = boost::none;
        return false;
    }

    m_hasHevcSupport = value.get().indexOf(kHevcCodecString) != -1;
    return true;
}

boost::optional<bool> VivotekResource::hasHevcSupport() const
{
    return m_hasHevcSupport;
}

bool VivotekResource::streamSupportsHevc(Qn::ConnectionRole role) const
{
    auto streamIndex = role == Qn::ConnectionRole::CR_LiveVideo ? 0 : 1;
    if (m_streamCodecCapabilities.size() < role + 1)
        return false;

    // Little hack here. Vivotek API can not report
    // presence of hevc support per stream. We consider the stream as HEVC one
    // if it supports H264 and the device supports HEVC in general.
    return hasHevcSupport()
        && m_streamCodecCapabilities[streamIndex].testFlag(StreamCodecCapability::h264);
}

bool VivotekResource::setHevcForStream(Qn::ConnectionRole role)
{
    const auto setUpHevcForStreamParameterName = kStreamCodecParameterTemplate
        .arg(getChannel())
        .arg(role == Qn::ConnectionRole::CR_LiveVideo ? 0 : 1);

    bool result = setVivotekParameter(
        setUpHevcForStreamParameterName,
        kHevcCodecString);

    if (!result)
        return false;

    return true;
}

bool VivotekResource::parseStreamCodecCapabilities(
    const QString& codecCapabilitiesString,
    std::vector<StreamCodecCapabilities>* outCapabilities) const
{
    NX_ASSERT(outCapabilities, lit("No output vector provided."));
    bool success = false;
    auto split = codecCapabilitiesString.split(L',');

    for (const auto& streamCapabilities: split)
    {
        StreamCodecCapabilityUnderlyingType capsEncoded =
            streamCapabilities.trimmed().toInt(&success);

        if (!success)
            return false;

        QFlags<StreamCodecCapability> caps;
        if (capsEncoded & (StreamCodecCapabilityUnderlyingType)StreamCodecCapability::h264)
            caps |= StreamCodecCapability::h264;
        if (capsEncoded & (StreamCodecCapabilityUnderlyingType)StreamCodecCapability::mjpeg)
            caps |= StreamCodecCapability::mjpeg;
        if (capsEncoded & (StreamCodecCapabilityUnderlyingType)StreamCodecCapability::mpeg4)
            caps |= StreamCodecCapability::mpeg4;
        if (capsEncoded & (StreamCodecCapabilityUnderlyingType)StreamCodecCapability::svc)
            caps |= StreamCodecCapability::svc;

        outCapabilities->push_back(caps);
    }

    return true;
}

void VivotekResource::tuneHttpClient(nx::network::http::HttpClient& httpClient) const
{
    auto auth = getAuth();
    httpClient.setSendTimeout(kHttpTimeout);
    httpClient.setMessageBodyReadTimeout(kHttpTimeout);
    httpClient.setResponseReadTimeout(kHttpTimeout);
    httpClient.setUserName(auth.user());
    httpClient.setUserPassword(auth.password());
}

bool VivotekResource::parseResponse(
    const nx::Buffer& response,
    QString* outName,
    QString* outValue) const
{
    auto split = response.trimmed().split(L'=');
    if (split.size() != 2)
        return false;

    *outName = QString::fromUtf8(split[0]);
    *outValue = QString::fromUtf8(split[1].mid(1, split[1].size() - 2));

    if (*outValue == kInvalid)
        return false;

    return true;
}

bool VivotekResource::doVivotekRequest(
    const nx::utils::Url& url,
    QString* outParameterName,
    QString* outParameterValue) const
{
    nx::network::http::HttpClient httpClient;
    tuneHttpClient(httpClient);

    if (!httpClient.doGet(url))
        return false;

    nx::Buffer response;
    while (!httpClient.eof())
        response.append(httpClient.fetchMessageBodyBuffer());

    if (!parseResponse(response, outParameterName, outParameterValue))
        return false;

    return true;
}

boost::optional<QString> VivotekResource::getVivotekParameter(const QString& param) const
{
    auto url = nx::utils::Url(getUrl());
    auto query = QUrlQuery(param);
    url.setPath(kGetParameterPath);
    url.setQuery(query);

    QString parameterName;
    QString parameterValue;

    if (!doVivotekRequest(url, &parameterName, &parameterValue))
        return boost::none;

    return parameterValue;
}

bool VivotekResource::setVivotekParameter(
    const QString& parameterName,
    const QString& parameterValue) const
{
    auto url = nx::utils::Url(getUrl());
    auto query = QUrlQuery(parameterName + lit("=") + parameterValue);
    url.setPath(kSetParameterPath);
    url.setQuery(query);

    QString dummyParameterName;
    QString dummyParameterValue;

    if (!doVivotekRequest(url, &dummyParameterName, &dummyParameterValue))
        return false;

    return true;
}

nx::vms::server::resource::StreamCapabilityMap VivotekResource::getStreamCapabilityMapFromDriver(
    StreamIndex streamIndex)
{
    QnMutexLocker lock(&m_mutex);
    using namespace nx::vms::server::resource;

    auto onvifResult = base_type::getStreamCapabilityMapFromDriver(streamIndex);
    QSet<QPair<int,int>> resolutions;
    for (const auto key: onvifResult.keys())
        resolutions.insert(QPair<int, int>(key.resolution.width(), key.resolution.height()));

    nx::vms::server::resource::StreamCapabilityMap result = onvifResult;
    if (m_hasHevcSupport && m_hasHevcSupport.get())
    {
        for (const auto& resolution: resolutions)
        {
            StreamCapabilityKey key;
            key.codec = kHevcCodecString.toUpper();
            key.resolution = QSize(resolution.first, resolution.second);
            result.insert(key, nx::media::CameraStreamCapability());
        }
    }
    return result;
}

} // namespace plugins
} // namespace vms::server
} // namespace nx

#endif // ENABLE_ONVIF
