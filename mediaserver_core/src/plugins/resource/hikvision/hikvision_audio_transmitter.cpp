#include <QtXml/QDomElement>

#include "hikvision_audio_transmitter.h"
#include "hikvision_parsing_utils.h"

#include <nx/utils/std/cpp14.h>

namespace {

const QString kTwoWayAudioPrefix = lit("/ISAPI/System/TwoWayAudio/channels/");

const QString kChannelStatusUrlTemplate = lit("%1");
const QString kOpenTwoWayAudioUrlTemplate = lit("%1/open");
const QString kCloseTwoWayAudioUrlTemplate = lit("%1/close");
const QString kTransmitTwoWayAudioUrlTemplate = lit("%1/audioData");

const QString kStatusCodeOk = lit("1");
const QString kSubStatusCodeOk = lit("ok");

const std::chrono::milliseconds kHttpHelperTimeout(4000);
const std::chrono::milliseconds kTransmissionTimeout(4000);

bool responseIsOk(const nx_http::Response* const response)
{
    return response->statusLine.statusCode == nx_http::StatusCode::ok;
}

} // namespace

using namespace nx::plugins;

HikvisionAudioTransmitter::HikvisionAudioTransmitter(QnSecurityCamResource* resource):
    base_type(resource)
{
}

bool HikvisionAudioTransmitter::isCompatible(const QnAudioFormat& format) const
{
    // Unused function
    return true;
}

void HikvisionAudioTransmitter::setChannelId(QString channelId)
{
    QnMutexLocker lock(&m_mutex);
    m_channelId = channelId;
}

bool HikvisionAudioTransmitter::sendData(const QnAbstractMediaDataPtr& data)
{
    return sendBuffer(m_socket.get(), data->data(), data->dataSize());
}

void HikvisionAudioTransmitter::prepareHttpClient(nx_http::AsyncHttpClientPtr httpClient)
{
    auto auth = m_resource->getAuth();

    httpClient->setUserName(auth.user());
    httpClient->setUserPassword(auth.password());
    httpClient->setDisablePrecalculatedAuthorization(false);
    httpClient->setAuthType(nx_http::AsyncHttpClient::AuthType::authBasic);

    openChannelIfNeeded();
}

bool HikvisionAudioTransmitter::isReadyForTransmission(
    nx_http::AsyncHttpClientPtr /*httpClient*/,
    bool /*isRetryAfterUnauthorizedResponse*/) const
{
    return true;
}

QUrl HikvisionAudioTransmitter::transmissionUrl() const
{
    QUrl url(m_resource->getUrl());
    url.setPath(kTwoWayAudioPrefix + kTransmitTwoWayAudioUrlTemplate.arg(m_channelId));

    return url;
}

std::chrono::milliseconds HikvisionAudioTransmitter::transmissionTimeout() const
{
    return kTransmissionTimeout;
}

nx_http::StringType HikvisionAudioTransmitter::contentType() const
{
    return nx_http::StringType("application/binary");
}

bool HikvisionAudioTransmitter::openChannelIfNeeded()
{
    auto auth = m_resource->getAuth();

    auto httpHelper = createHttpHelper();

    QUrl url(m_resource->getUrl());
    auto channelStatusPath = kTwoWayAudioPrefix + kChannelStatusUrlTemplate.arg(m_channelId);
    auto channelOpenPath = kTwoWayAudioPrefix + kOpenTwoWayAudioUrlTemplate.arg(m_channelId);

    url.setPath(channelStatusPath);

    auto result = httpHelper->doGet(url);
    if (!result)
        return false;

    nx_http::BufferType messageBody;
    while (!httpHelper->eof())
        messageBody.append(httpHelper->fetchMessageBodyBuffer());

    auto channelStatus = hikvision::parseChannelStatusResponse(messageBody);
    auto format = hikvision::toAudioFormat(
        channelStatus->audioCompression,
        channelStatus->sampleRateKHz);

    if (format.sampleRate() != m_outputFormat.sampleRate() || 
        format.codec() != m_outputFormat.codec())
    {
        m_outputFormat = format;
        base_type::prepare();
    }

    if (channelStatus && channelStatus->enabled)
        return true;

    url.setPath(channelOpenPath);
    result = httpHelper->doPut(
        url,
        nx_http::StringType(),
        nx_http::StringType());

    if (!result)
        return false;

    messageBody.clear();

    while (!httpHelper->eof())
        messageBody.append(httpHelper->fetchMessageBodyBuffer());

    auto openResult = hikvision::parseOpenChannelResponse(messageBody);

    if (!openResult)
        return false;

    return true;
}

bool HikvisionAudioTransmitter::closeChannel()
{
    auto auth = m_resource->getAuth();

    auto httpHelper = createHttpHelper();

    QUrl url(m_resource->getUrl());
    url.setPath(kTwoWayAudioPrefix + kCloseTwoWayAudioUrlTemplate.arg(m_channelId));

    auto result = httpHelper->doPut(
        url,
        nx_http::StringType(),
        nx_http::StringType());

    if (!result || !responseIsOk(httpHelper->response()))
        return false;

    nx_http::StringType messageBody;
    while (!httpHelper->eof())
        messageBody.append(httpHelper->fetchMessageBodyBuffer());

    auto response = hikvision::parseCommonResponse(messageBody);

    if (!response 
        || response->statusCode != kStatusCodeOk 
        || response->subStatusCode.toLower() != kSubStatusCodeOk)
    {
        return false;
    }

    return true;
}

std::unique_ptr<nx_http::HttpClient> HikvisionAudioTransmitter::createHttpHelper()
{
    auto auth = m_resource->getAuth();

    auto httpHelper = std::make_unique<nx_http::HttpClient>();
    httpHelper->setUserName(auth.user());
    httpHelper->setUserPassword(auth.password());
    httpHelper->setResponseReadTimeoutMs(kHttpHelperTimeout.count());
    httpHelper->setSendTimeoutMs(kHttpHelperTimeout.count());
    httpHelper->setMessageBodyReadTimeoutMs(kHttpHelperTimeout.count());

    return httpHelper;
}
