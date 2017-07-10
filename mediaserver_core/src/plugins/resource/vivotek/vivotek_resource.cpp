#include "vivotek_resource.h"

#include <chrono>

#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

const uint8_t kMpeg4Mask = 0b1;
const uint8_t kMjpegMask = 0b10;
const uint8_t kH264Mask = 0b100;
const uint8_t kSvcMask = 0b1000;

const QString kGetParameterPath = lit("/cgi-bin/admin/getparam.cgi");
const QString kSetParameterPath = lit("/cgi-bin/admin/setparam.cgi");
const QString kInvalid = lit("ERR_INVALID");
const QString kHevcCodecString = lit("h265");

const QString kGeneralCodecCapability = lit("capability_videoin_codec");
// codec capability per stream
const QString kStreamCodecCapabilities = lit("capability_videoin_streamcodec");
const QString kStreamCodecParameterTemplate = lit("videoin_c%1_s%2_codectype");

const std::chrono::milliseconds kHttpTimeout(5000);

} // namespace

CameraDiagnostics::Result VivotekResource::initializeMedia(const CapabilitiesResp& onvifCapabilities)
{
    auto result = base_type::initializeMedia(onvifCapabilities);
    if (!result)
        return result;

    fetchHevcSupport();
    auto hevcIsSupported = hasHevcSupport();
    
    if (!hevcIsSupported || !hevcIsSupported.get())
        return CameraDiagnostics::NoErrorResult();

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
        m_streamCodecCapabilities);

    if (!streamCodecCapabilitiesParsed)
    {
        return CameraDiagnostics::CameraResponseParseErrorResult(
            kStreamCodecCapabilities,
            lit("Stream codec capabilities"));
    }

    return CameraDiagnostics::NoErrorResult();
}

void VivotekResource::afterConfigureStream(Qn::ConnectionRole role)
{
    if (streamSupportsHevc(role))
        setHevcForStream(role);

    base_type::afterConfigureStream(role);
}

bool VivotekResource::fetchHevcSupport()
{
    if (m_hasHevcSupport)
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
    return hasHevcSupport() && m_streamCodecCapabilities[streamIndex].h264;
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
    std::vector<VivotekResource::StreamCodecCapabilities>& outCapabilities) const
{
    bool success = false;
    auto split = codecCapabilitiesString.split(L',');
    
    for (const auto& streamCapabilities: split)
    {
        auto capsEncoded = streamCapabilities.trimmed().toUInt(&success);
        if (!success)
            return false;

        StreamCodecCapabilities caps;
        caps.h264 = capsEncoded | kH264Mask;
        caps.mjpeg = capsEncoded | kMjpegMask;
        caps.mpeg4 = capsEncoded | kMpeg4Mask;
        caps.svc = capsEncoded | kSvcMask;

        outCapabilities.push_back(caps);
    }

    return true;
}

void VivotekResource::tuneHttpClient(nx_http::HttpClient& httpClient) const
{
    auto auth = getAuth();
    httpClient.setSendTimeoutMs(kHttpTimeout.count());
    httpClient.setMessageBodyReadTimeoutMs(kHttpTimeout.count());
    httpClient.setResponseReadTimeoutMs(kHttpTimeout.count());
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
    const QUrl& url,
    QString* outParameterName,
    QString* outParameterValue) const
{
    nx_http::HttpClient httpClient;
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
    auto url = QUrl(getUrl());
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
    auto url = QUrl(getUrl());
    auto query = QUrlQuery(parameterName + lit("=") + parameterValue);
    url.setPath(kSetParameterPath);
    url.setQuery(query);

    QString dummyParameterName;
    QString dummyParameterValue;

    if (!doVivotekRequest(url, &dummyParameterName, &dummyParameterValue))
        return false;

    return true;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
