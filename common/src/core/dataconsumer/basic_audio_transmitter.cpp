#if defined(ENABLE_DATA_PROVIDERS)

#include "basic_audio_transmitter.h"

namespace
{
    const std::chrono::milliseconds kTransmissionTimeout(3000);
} // namespace

QnBasicAudioTransmitter::QnBasicAudioTransmitter(QnSecurityCamResource* res):
    BaseHttpAudioTransmitter(res)
{
}

QnBasicAudioTransmitter::~QnBasicAudioTransmitter()
{
    stop();
}

bool QnBasicAudioTransmitter::sendData(
    const QnAbstractMediaDataPtr& data)
{
    return base_type::sendBuffer(m_socket.get(), data->data(), data->dataSize());
}

void QnBasicAudioTransmitter::setAuthPolicy(AuthPolicy value)
{
    m_authPolicy = value;
}

void QnBasicAudioTransmitter::prepareHttpClient(const nx_http::AsyncHttpClientPtr& httpClient)
{
    auto auth = m_resource->getAuth();
    httpClient->setUserName(auth.user());
    httpClient->setUserPassword(auth.password());
    if (m_authPolicy == AuthPolicy::basicAuth)
        httpClient->setAuthType(nx_http::AsyncHttpClient::AuthType::authBasic);
    else
        httpClient->setDisablePrecalculatedAuthorization(true);
}

bool QnBasicAudioTransmitter::isReadyForTransmission(
    nx_http::AsyncHttpClientPtr /*httpClient*/,
    bool isRetryAfterUnauthorizedResponse) const
{
    auto auth = m_resource->getAuth();
    bool noAuth = (auth.user().isEmpty() && auth.password().isEmpty()) ||
        m_authPolicy == AuthPolicy::noAuth ||
        m_authPolicy == AuthPolicy::basicAuth;
    return isRetryAfterUnauthorizedResponse || noAuth;
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
    return m_contentType;
}

#endif // ENABLE_DATA_PROVIDERS

