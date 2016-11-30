#ifdef ENABLE_ONVIF

#include "hikvision_onvif_resource.h"
#include <utils/camera/camera_diagnostics.h>
#include <QtXml/QDomElement>
#include "hikvision_audio_transmitter.h"

namespace {

    const int kRequestTimeoutMs = 4 * 1000;

    bool isResponseOK(const nx_http::HttpClient* client)
    {
        if (!client->response())
            return false;
        return client->response()->statusLine.statusCode == nx_http::StatusCode::ok;
    }

    QnAudioFormat toAudioFormat(const QString& codecName, int bitrateKbps)
    {
        QnAudioFormat result;
        if (codecName == lit("G.711alaw"))
        {
            result.setSampleRate(8000);
            result.setCodec("ALAW");
        }
        else if (codecName == lit("G.711ulaw"))
        {
            result.setSampleRate(8000);
            result.setCodec("MULAW");
        }
        else if (codecName == lit("G.726"))
        {
            result.setSampleRate(8000);
            result.setCodec("G726");
        }
        else if (codecName == lit("AAC"))
        {
            result.setSampleRate(16000);
            result.setCodec("AAC");
        }
        if (bitrateKbps > 0)
            result.setSampleRate(bitrateKbps); //< override default value
        return result;
    }

} // namespace

QnHikvisionOnvifResource::QnHikvisionOnvifResource():
    QnPlOnvifResource(),
    m_audioTransmitter(new QnHikvisionAudioTransmitter(this))
{
}

QnHikvisionOnvifResource::~QnHikvisionOnvifResource()
{
    m_audioTransmitter.reset();
}

CameraDiagnostics::Result QnHikvisionOnvifResource::initInternal()
{
    auto result = QnPlOnvifResource::initInternal();
    if (result != CameraDiagnostics::NoErrorResult())
        return result;

    result = initialize2WayAudio();
    if (result != CameraDiagnostics::NoErrorResult())
        return result;

    return CameraDiagnostics::NoErrorResult();
}

std::unique_ptr<nx_http::HttpClient> QnHikvisionOnvifResource::getHttpClient()

{
    std::unique_ptr<nx_http::HttpClient> httpClient(new nx_http::HttpClient);
    httpClient->setResponseReadTimeoutMs(kRequestTimeoutMs);
    httpClient->setSendTimeoutMs(kRequestTimeoutMs);
    httpClient->setMessageBodyReadTimeoutMs(kRequestTimeoutMs);
    httpClient->setUserName(getAuth().user());
    httpClient->setUserPassword(getAuth().password());

    return std::move(httpClient);
}

CameraDiagnostics::Result QnHikvisionOnvifResource::initialize2WayAudio()
{
    auto httpClient = getHttpClient();

    QUrl requestUrl(getUrl());
    requestUrl.setPath("/ISAPI/System/TwoWayAudio/channels");
    requestUrl.setHost(getHostAddress());
    requestUrl.setPort(QUrl(getUrl()).port(80));

    if (!httpClient->doGet(requestUrl) || !isResponseOK(httpClient.get()))
    {
        return CameraDiagnostics::CameraResponseParseErrorResult(
            requestUrl.toString(QUrl::RemovePassword),
            "Read two way audio info");
    }

    QByteArray data;
    while (!httpClient->eof())
        data.append(httpClient->fetchMessageBodyBuffer());
    if (data.isEmpty())
        return CameraDiagnostics::NoErrorResult(); //< no 2-way-audio cap

    QDomDocument doc;
    doc.setContent(data);
    QDomElement docElem = doc.documentElement();

    QString outputId;
    QStringList supportedCodecs;
    int bitrateKbps = 0;

    QDomNode node = docElem.firstChild();
    while (!node.isNull())
    {
        QDomElement element = node.toElement();
        if (!element.isNull() && element.tagName() == "TwoWayAudioChannel")
        {
            auto params = element.firstChild();
            while (!params.isNull())
            {
                element = params.toElement();
                if (element.isNull())
                    continue;
                if (element.nodeName() == "id")
                    outputId = element.text();
                else if (element.nodeName() == "audioCompressionType")
                    supportedCodecs = element.text().split(",");
                else if (element.nodeName() == "audioBitRate")
                    bitrateKbps = element.text().toInt();

                params = params.nextSibling();
            }
        }
        node = node.nextSibling();
    }

    if (outputId.isEmpty())
        return CameraDiagnostics::NoErrorResult(); //< no audio outputs

    for (const auto& codec: supportedCodecs)
    {
        QnAudioFormat outputFormat = toAudioFormat(codec, bitrateKbps);
        if (m_audioTransmitter->isCompatible(outputFormat))
        {
            m_audioTransmitter->setOutputFormat(outputFormat);
            m_audioTransmitter->setOutputId(outputId);
            setCameraCapabilities(getCameraCapabilities() | Qn::AudioTransmitCapability);
        }
    }

    return CameraDiagnostics::NoErrorResult();
}

QnAudioTransmitterPtr QnHikvisionOnvifResource::getAudioTransmitter()
{
    if (!isInitialized() || !m_audioTransmitter->isInitialized())
        return nullptr;

    return m_audioTransmitter;
}

#endif  //ENABLE_ONVIF
