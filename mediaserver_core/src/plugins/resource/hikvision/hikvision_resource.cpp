#ifdef ENABLE_ONVIF

#include "hikvision_resource.h"
#include "hikvision_audio_transmitter.h"
#include "hikvision_utils.h"
#include "hikvision_hevc_stream_reader.h"

#include <QtXml/QDomElement>

#include <boost/optional.hpp>
#include <utils/camera/camera_diagnostics.h>
#include <common/common_module.h>
#include <common/static_common_module.h>
#include <core/resource_management/resource_data_pool.h>

namespace {

const std::chrono::milliseconds kRequestTimeout(4000);

bool isResponseOK(const nx_http::HttpClient* client)
{
    if (!client->response())
        return false;
    return client->response()->statusLine.statusCode == nx_http::StatusCode::ok;
}

const std::array<Qn::ConnectionRole, 2> kRoles = {
    Qn::ConnectionRole::CR_LiveVideo,
    Qn::ConnectionRole::CR_SecondaryLiveVideo};

} // namespace

namespace nx {
namespace mediaserver_core {
namespace plugins {

using namespace nx::mediaserver_core::plugins::hikvision;

HikvisionResource::HikvisionResource():
    QnPlOnvifResource()
{
    m_audioTransmitter.reset(new HikvisionAudioTransmitter(this));
}

HikvisionResource::~HikvisionResource()
{
    m_audioTransmitter.reset();
}

CameraDiagnostics::Result HikvisionResource::initInternal()
{
    auto result = QnPlOnvifResource::initInternal();
    if (result.errorCode != CameraDiagnostics::ErrorCode::noError)
        return result;

    initialize2WayAudio();
    saveParams();

    return CameraDiagnostics::NoErrorResult();
}

QnAbstractStreamDataProvider* HikvisionResource::createLiveDataProvider()
{
    if (m_hevcSupported)
        return new plugins::HikvisionHevcStreamReader(toSharedPointer(this));
    
    return base_type::createLiveDataProvider();
}

CameraDiagnostics::Result HikvisionResource::initializeMedia(
    const CapabilitiesResp& onvifCapabilities)
{
    auto resourceData = qnStaticCommon->dataPool()->data(toSharedPointer(this));
    bool hevcIsDisabled = resourceData.value<bool>(Qn::DISABLE_HEVC_PARAMETER_NAME, false);

    if (!hevcIsDisabled)
    {
        for (const auto& role : kRoles)
        {
            hikvision::ChannelCapabilities channelCapabilities;
            auto result = fetchChannelCapabilities(role, &channelCapabilities);
            if (!result)
                return result;

            m_channelCapabilitiesByRole[role] = channelCapabilities;
            m_hevcSupported = hikvision::codecSupported(
                AV_CODEC_ID_HEVC,
                channelCapabilities);
            if (m_hevcSupported && role == Qn::ConnectionRole::CR_LiveVideo)
            {
                setProperty(Qn::HAS_DUAL_STREAMING_PARAM_NAME, 1);
                if (!channelCapabilities.resolutions.empty())
                {
                    const auto primaryResolution = channelCapabilities.resolutions.front();
                    setPrimaryResolution(primaryResolution);
                    if (qFuzzyIsNull(customAspectRatio()))
                        setCustomAspectRatio(primaryResolution.width() / (qreal)primaryResolution.height());
                }
                if (!channelCapabilities.fps.empty())
                    setMaxFps(channelCapabilities.fps[0] / 100);
            }
        }
    }

    if (!m_hevcSupported)
        return base_type::initializeMedia(onvifCapabilities);

    return CameraDiagnostics::NoErrorResult();
}

std::unique_ptr<nx_http::HttpClient> HikvisionResource::getHttpClient()
{
    std::unique_ptr<nx_http::HttpClient> httpClient(new nx_http::HttpClient);
    httpClient->setResponseReadTimeoutMs(kRequestTimeout.count());
    httpClient->setSendTimeoutMs(kRequestTimeout.count());
    httpClient->setMessageBodyReadTimeoutMs(kRequestTimeout.count());
    httpClient->setUserName(getAuth().user());
    httpClient->setUserPassword(getAuth().password());

    return std::move(httpClient);
}

CameraDiagnostics::Result HikvisionResource::fetchChannelCapabilities(
    Qn::ConnectionRole role,
    ChannelCapabilities* outCapabilities)
{
    auto url = nx::utils::Url(getUrl());
    url.setPath(kCapabilitiesRequestPathTemplate.arg(
        buildChannelNumber(role, getChannel())));

    nx::Buffer response;
    nx_http::StatusCode::Value statusCode;
    if (!doGetRequest(url, getAuth(), &response, &statusCode))
    {
        if (statusCode == nx_http::StatusCode::Value::unauthorized)
            return CameraDiagnostics::NotAuthorisedResult(url.toString());

        return CameraDiagnostics::RequestFailedResult(
            lit("Fetch channel capabilities"),
            lit("Request failed"));
    }

    if (!parseChannelCapabilitiesResponse(response, outCapabilities))
    {
        return CameraDiagnostics::CameraResponseParseErrorResult(
            url.toString(),
            lit("Fetch camera capabilities"));
    }

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result HikvisionResource::initialize2WayAudio()
{
    auto httpClient = getHttpClient();

    nx::utils::Url requestUrl(getUrl());
    requestUrl.setPath(lit("/ISAPI/System/TwoWayAudio/channels"));
    requestUrl.setHost(getHostAddress());
    requestUrl.setPort(QUrl(getUrl()).port(nx_http::DEFAULT_HTTP_PORT));

    if (!httpClient->doGet(requestUrl) || !isResponseOK(httpClient.get()))
    {
        return CameraDiagnostics::CameraResponseParseErrorResult(
            requestUrl.toString(QUrl::RemovePassword),
            lit("Read two way audio info"));
    }

    QByteArray data;
    while (!httpClient->eof())
        data.append(httpClient->fetchMessageBodyBuffer());

    if (data.isEmpty())
        return CameraDiagnostics::NoErrorResult(); //< no 2-way-audio cap

    auto channels = parseAvailableChannelsResponse(data);

    if (channels.empty())
        return CameraDiagnostics::NoErrorResult(); //< no 2-way-audio cap

    boost::optional<ChannelStatusResponse> channel(boost::none);
    for (const auto& ch: channels)
    {
        if (!ch.id.isEmpty() && !ch.audioCompression.isEmpty())
        {
            channel = ch;
            break;
        }
    }

    if (!channel)
        return CameraDiagnostics::NoErrorResult();

    QnAudioFormat outputFormat = toAudioFormat(
        channel->audioCompression,
        channel->sampleRateKHz);

    if (m_audioTransmitter->isCompatible(outputFormat))
    {
        m_audioTransmitter->setOutputFormat(outputFormat);

        auto hikTransmitter = std::dynamic_pointer_cast<HikvisionAudioTransmitter>(
            m_audioTransmitter);

        if (hikTransmitter)
        {
            hikTransmitter->setChannelId(channel->id);
            hikTransmitter->setAudioUploadHttpMethod(nx_http::Method::put);
        }

        setCameraCapabilities(getCameraCapabilities() | Qn::AudioTransmitCapability);
    }

    return CameraDiagnostics::NoErrorResult();
}

boost::optional<ChannelCapabilities> HikvisionResource::channelCapabilities(
    Qn::ConnectionRole role)
{
    auto itr = m_channelCapabilitiesByRole.find(role);
    if (itr == m_channelCapabilitiesByRole.end())
        return boost::none;

    return itr->second;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif  //ENABLE_ONVIF
