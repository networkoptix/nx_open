#ifdef ENABLE_ONVIF

#include "hikvision_onvif_resource.h"
#include <nx/network/http/httpclient.h>
#include <utils/camera/camera_diagnostics.h>
#include <QtXml/QDomElement>

namespace {

    const int kRequestTimeoutMs = 4 * 1000;

    bool isResponseOK(const nx_http::HttpClient& client)
    {
        if (!client.response())
            return false;
        return client.response()->statusLine.statusCode == nx_http::StatusCode::ok;
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

QnHikvisionOnvifResource::QnHikvisionOnvifResource(): QnPlOnvifResource()
{

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

CameraDiagnostics::Result QnHikvisionOnvifResource::initialize2WayAudio()
{
    nx_http::HttpClient httpClient;
    httpClient.setResponseReadTimeoutMs(kRequestTimeoutMs);
    httpClient.setSendTimeoutMs(kRequestTimeoutMs);
    httpClient.setMessageBodyReadTimeoutMs(kRequestTimeoutMs);

    httpClient.setUserName(getAuth().user());
    httpClient.setUserPassword(getAuth().password());



    QUrl requestUrl(getUrl());
    requestUrl.setPath("/ISAPI/System/TwoWayAudio/channels");
    requestUrl.setHost(getHostAddress());
    requestUrl.setPort(QUrl(getUrl()).port(80));

    if (!httpClient.doGet(requestUrl) || !isResponseOK(httpClient))
    {
        return CameraDiagnostics::CameraResponseParseErrorResult(
            requestUrl.toString(QUrl::RemovePassword),
            "Read two way audio info");
    }

    QByteArray data;
    while (!httpClient.eof())
        data.append(httpClient.fetchMessageBodyBuffer());
    if (data.isEmpty())
        return CameraDiagnostics::NoErrorResult(); //< no 2-way-audio cap

    qWarning() << data;
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

    for (const auto& codec: supportedCodecs)
    {
        QnAudioFormat outputFormat = toAudioFormat(codec, bitrateKbps);
        if (m_audioTransmitter->isCompatible(outputFormat))
        {
            m_audioTransmitter->setOutputFormat(outputFormat);
            setCameraCapabilities(getCameraCapabilities() | Qn::AudioTransmitCapability);
        }
    }

    return CameraDiagnostics::NoErrorResult();
}

#endif  //ENABLE_ONVIF
