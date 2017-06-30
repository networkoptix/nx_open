#include "basic_audio_transmitter.h"

namespace
{
    const std::chrono::milliseconds kTransmissionTimeout(3000);
} // namespace

QnBasicAudioTransmitter::QnBasicAudioTransmitter(QnSecurityCamResource* res):
    BaseHttpAudioTransmitter(res),
    m_noAuth(false)
{
}

bool QnBasicAudioTransmitter::sendData(
    const QnAbstractMediaDataPtr& data)
{
    return base_type::sendBuffer(m_socket.get(), data->data(), data->dataSize());
}

void QnBasicAudioTransmitter::prepareHttpClient(const nx_http::AsyncHttpClientPtr& httpClient)
{
    auto auth = m_resource->getAuth();
    m_noAuth = auth.user().isEmpty() && auth.password().isEmpty();

    httpClient->setUserName(auth.user());
    httpClient->setUserPassword(auth.password());
    httpClient->setDisablePrecalculatedAuthorization(true);
}

bool QnBasicAudioTransmitter::isReadyForTransmission(
    nx_http::AsyncHttpClientPtr /*httpClient*/,
    bool isRetryAfterUnauthorizedResponse) const
{
    return isRetryAfterUnauthorizedResponse || m_noAuth;
}

QUrl QnBasicAudioTransmitter::transmissionUrl() const
{
    return m_url;
}

void QnBasicAudioTransmitter::setTransmissionUrl(const QUrl& url)
{
    m_url = url;
}

std::chrono::milliseconds QnBasicAudioTransmitter::transmissionTimeout() const
{
    return kTransmissionTimeout;
}

void QnBasicAudioTransmitter::setContentType(const nx_http::StringType& contentType)
{
    m_contentType = contentType;
}

nx_http::StringType QnBasicAudioTransmitter::contentType() const
{
    if (!m_contentType.isNull())
        return m_contentType;

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

