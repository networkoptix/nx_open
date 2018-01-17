#include "axis_audio_transmitter.h"

namespace
{
    const QString kAxisAudioTransmitUrl("/axis-cgi/audio/transmit.cgi");
    const std::chrono::milliseconds kTransmissionTimeout(3000);
} // namespace

QnAxisAudioTransmitter::QnAxisAudioTransmitter(QnSecurityCamResource* res):
    BaseHttpAudioTransmitter(res),
    m_noAuth(false)
{
}

bool QnAxisAudioTransmitter::isCompatible(const QnAudioFormat& format) const
{
    return
        format.codec() == "AAC" ||
        //format.codec() == "G726" ||
        format.codec() == "MULAW";
}

bool QnAxisAudioTransmitter::sendData(
    const QnAbstractMediaDataPtr& data)
{
    return base_type::sendBuffer(m_socket.get(), data->data(), data->dataSize());
}

void QnAxisAudioTransmitter::prepareHttpClient(const nx::network::http::AsyncHttpClientPtr& httpClient)
{
    auto auth = m_resource->getAuth();
    m_noAuth = auth.user().isEmpty() && auth.password().isEmpty();

    httpClient->setUserName(auth.user());
    httpClient->setUserPassword(auth.password());
    httpClient->setDisablePrecalculatedAuthorization(true);
}

bool QnAxisAudioTransmitter::isReadyForTransmission(
    nx::network::http::AsyncHttpClientPtr /*httpClient*/,
    bool isRetryAfterUnauthorizedResponse) const
{
    return isRetryAfterUnauthorizedResponse || m_noAuth;
}

nx::utils::Url QnAxisAudioTransmitter::transmissionUrl() const
{
    nx::utils::Url url(m_resource->getUrl());

    url.setScheme(lit("http"));
    if (url.host().isEmpty())
        url.setHost(m_resource->getHostAddress());

    url.setPath(kAxisAudioTransmitUrl);

    return url;
}

std::chrono::milliseconds QnAxisAudioTransmitter::transmissionTimeout() const
{
    return kTransmissionTimeout;
}

nx::network::http::StringType QnAxisAudioTransmitter::contentType() const
{
    if (m_outputFormat.codec() == "MULAW")
    {
        if (m_outputFormat.sampleRate() == 8000)
            return QByteArray("audio/basic");
        else
            return lit("audio/axis-mulaw-%1")
            .arg((m_outputFormat.sampleRate() * 8) / 1000)
            .toLatin1();
    }
    else if (m_outputFormat.codec() == "G726")
    {
        return QByteArray("audio/G726-32");
    }
    else if (m_outputFormat.codec() == "AAC")
    {
        return QByteArray("audio/mpeg4-generic");
    }
    else
    {
        return QByteArray("audio/basic");
    }
}

