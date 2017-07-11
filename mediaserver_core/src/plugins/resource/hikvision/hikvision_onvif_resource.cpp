#ifdef ENABLE_ONVIF

#include <QtXml/QDomElement>

#include <boost/optional.hpp>
#include <utils/camera/camera_diagnostics.h>

#include "hikvision_onvif_resource.h"
#include "hikvision_audio_transmitter.h"
#include "hikvision_parsing_utils.h"

namespace {

const std::chrono::milliseconds kRequestTimeout(4000);

bool isResponseOK(const nx_http::HttpClient* client)
{
    if (!client->response())
        return false;
    return client->response()->statusLine.statusCode == nx_http::StatusCode::ok;
}

} // namespace

QnHikvisionOnvifResource::QnHikvisionOnvifResource():
    QnPlOnvifResource(),
    m_audioTransmitter(new HikvisionAudioTransmitter(this))
{
}

QnHikvisionOnvifResource::~QnHikvisionOnvifResource()
{
    m_audioTransmitter.reset();
}

CameraDiagnostics::Result QnHikvisionOnvifResource::initInternal()
{
    auto result = QnPlOnvifResource::initInternal();
    if (result.errorCode != CameraDiagnostics::ErrorCode::noError)
        return result;

    initialize2WayAudio();
    saveParams();

    return CameraDiagnostics::NoErrorResult();
}

std::unique_ptr<nx_http::HttpClient> QnHikvisionOnvifResource::getHttpClient()
{
    std::unique_ptr<nx_http::HttpClient> httpClient(new nx_http::HttpClient);
    httpClient->setResponseReadTimeoutMs(kRequestTimeout.count());
    httpClient->setSendTimeoutMs(kRequestTimeout.count());
    httpClient->setMessageBodyReadTimeoutMs(kRequestTimeout.count());
    httpClient->setUserName(getAuth().user());
    httpClient->setUserPassword(getAuth().password());

    return std::move(httpClient);
}

CameraDiagnostics::Result QnHikvisionOnvifResource::initialize2WayAudio()
{
    auto httpClient = getHttpClient();

    QUrl requestUrl(getUrl());
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

    auto channels = nx::plugins::hikvision::parseAvailableChannelsResponse(data);

    if (channels.empty())
        return CameraDiagnostics::NoErrorResult(); //< no 2-way-audio cap

    boost::optional<nx::plugins::hikvision::ChannelStatusResponse> channel(boost::none);
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

    QnAudioFormat outputFormat = nx::plugins::hikvision::toAudioFormat(
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

QnAudioTransmitterPtr QnHikvisionOnvifResource::getAudioTransmitter()
{
    if (!isInitialized())
        return nullptr;

    return m_audioTransmitter;
}

#endif  //ENABLE_ONVIF
