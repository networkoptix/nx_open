#include <QtXml/QDomElement>

#include "hikvision_audio_transmitter.h"

#include <nx/network/http/httpclient.h>

namespace {

const QString kTwoWayAudioPrefix = lit("/ISAPI/System/TwoWayAudio/channels/");

const QString kChannelStatusUrlTemplate = lit("%1");
const QString kOpenTwoWayAudioUrlTemplate = lit("%1/open");
const QString kCloseTwoWayAudioUrlTemplate = lit("%1/close");
const QString kTransmitTwoWayAudioUrlTemplate = lit("%1/audioData");

const QString kStatusCodeOk = lit("1");
const QString kSubStatusCodeOk = lit("ok");

const std::chrono::milliseconds kTransmissionTimeout(4000);

bool responseIsOk(const nx_http::Response* const response)
{
    return response->statusLine.statusCode == nx_http::StatusCode::ok;
}

} // namespace

HikvisionAudioTransmitter::HikvisionAudioTransmitter(QnSecurityCamResource* resource):
    base_type(resource)
{
}

bool HikvisionAudioTransmitter::isCompatible(const QnAudioFormat& format) const
{
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
    httpClient->setAllowPrecalculatedBasicAuth(true);

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

    nx_http::HttpClient httpHelper;
    httpHelper.setUserName(auth.user());
    httpHelper.setUserPassword(auth.password());

    QUrl url(m_resource->getUrl());
    auto channelStatusPath = kTwoWayAudioPrefix + kChannelStatusUrlTemplate.arg(m_channelId);
    auto channelOpenPath = kTwoWayAudioPrefix + kOpenTwoWayAudioUrlTemplate.arg(m_channelId);

    url.setPath(channelStatusPath);

    auto result = httpHelper.doGet(url);
    if (!result)
        return false;

    nx_http::BufferType messageBody;
    while (!httpHelper.eof())
        messageBody.append(httpHelper.fetchMessageBodyBuffer());

    auto channelStatus = parseChannelStatusResponse(messageBody);

    if (channelStatus && channelStatus->enabled)
        return true;

    url.setPath(channelOpenPath);
    result = httpHelper.doPut(
        url,
        nx_http::StringType(),
        nx_http::StringType());

    if (!result)
        return false;

    messageBody.clear();

    while (!httpHelper.eof())
        messageBody.append(httpHelper.fetchMessageBodyBuffer());

    auto openResult = parseOpenChannelResponse(messageBody);

    if (!openResult)
        return false;

    return true;
}

bool HikvisionAudioTransmitter::closeChannel()
{
    auto auth = m_resource->getAuth();

    nx_http::HttpClient httpHelper;
    httpHelper.setUserName(auth.user());
    httpHelper.setUserPassword(auth.password());

    QUrl url(m_resource->getUrl());
    url.setPath(kTwoWayAudioPrefix + kCloseTwoWayAudioUrlTemplate.arg(m_channelId));

    auto result = httpHelper.doPut(
        url,
        nx_http::StringType(),
        nx_http::StringType());

    if (!result || !responseIsOk(httpHelper.response()))
        return false;

    nx_http::StringType messageBody;
    while (!httpHelper.eof())
        messageBody.append(httpHelper.fetchMessageBodyBuffer());

    auto response = parseCommonResponse(messageBody);

    if (!response 
        || response->statusCode != kStatusCodeOk 
        || response->subStatusCode.toLower() != kSubStatusCodeOk)
    {
        return false;
    }

    return true;
}

boost::optional<HikvisionAudioTransmitter::ChannelStatusResponse>
HikvisionAudioTransmitter::parseChannelStatusResponse(nx_http::StringType message) const
{
    ChannelStatusResponse response;
    QDomDocument doc;
    bool status;

    doc.setContent(message);
    auto element = doc.documentElement();
    if (!element.isNull() && element.tagName() == lit("TwoWayAudioChannel"))
    {
        auto params = element.firstChild();
        while (!params.isNull())
        {
            element = params.toElement();
            if (element.isNull())
                continue;

            auto nodeName = element.nodeName();

            if (nodeName == lit("id"))
                response.id = element.text();
            else if (nodeName == lit("enabled"))
                response.enabled = element.text() == lit("true");
            else if (nodeName == lit("audioCompressionType"))
                response.audioCompression = element.text();
            else if (nodeName == lit("audioInputType"))
                response.audioInputType = element.text();
            else if (nodeName == lit("speakerVolume"))
            {
                response.speakerVolume = element.text().toInt(&status);
                if (!status)
                    return boost::none;
            }
            else if (nodeName == lit("noisereduce"))
                response.noiseReduce = element.text() == lit("true");

            params = params.nextSibling();
        }
    }
    else
    {
        return boost::none;
    }

    return response;
}

boost::optional<HikvisionAudioTransmitter::OpenChannelResponse>
HikvisionAudioTransmitter::parseOpenChannelResponse(nx_http::StringType message) const
{
    OpenChannelResponse response;

    QDomDocument doc;
    bool status;

    doc.setContent(message);

    auto element = doc.documentElement();
    if (!element.isNull() && element.tagName() == lit("TwoWayAudioSession"))
    {
        auto sessionIdNode =  element.firstChild();
        if (sessionIdNode.isNull() || !sessionIdNode.isElement())
            return boost::none;

        auto sessionId = sessionIdNode.toElement();

        if (sessionId.tagName() != lit("sessionId"))
            return boost::none;

        response.sessionId = sessionId.text();
    }
    else
    {
        return boost::none;
    }

    return response;
}

boost::optional<HikvisionAudioTransmitter::CommonResponse> 
HikvisionAudioTransmitter::parseCommonResponse(nx_http::StringType message) const
{
    CommonResponse response;
    QDomDocument doc;
    bool status;

    doc.setContent(message);

    auto element = doc.documentElement();
    if (!element.isNull() && element.tagName() == lit("ResponseStatus"))
    {
        auto params = element.firstChild();
        while (!params.isNull())
        {
            element = params.toElement();
            if (element.isNull())
                continue;

            auto nodeName = element.nodeName();

            if (nodeName == lit("statusCode"))
                response.statusCode = element.text();
            else if (nodeName == lit("statusString"))
                response.statusString = element.text();
            else if (nodeName == lit("subStatusCode"))
                response.subStatusCode = element.text();
           
            params = params.nextSibling();
        }
    }
    else
    {
        return boost::none;
    }

    return response;
}