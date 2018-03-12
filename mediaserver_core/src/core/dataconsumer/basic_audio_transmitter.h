#pragma once

#if defined(ENABLE_DATA_PROVIDERS)

#include <core/dataconsumer/base_http_audio_transmitter.h>
#include <core/resource/security_cam_resource.h>
#include <nx/network/deprecated/asynchttpclient.h>

class QnBasicAudioTransmitter: public BaseHttpAudioTransmitter
{
    using base_type = BaseHttpAudioTransmitter;
    Q_OBJECT
public:
    enum class AuthPolicy
    {
        noAuth,
        basicAuth,
        digestAndBasicAuth
    };

    QnBasicAudioTransmitter(QnSecurityCamResource* res);
    virtual ~QnBasicAudioTransmitter() override;
    void setTransmissionUrl(const nx::utils::Url &url);
    void setContentType(const nx::network::http::StringType& contentType);
    void setAuthPolicy(AuthPolicy value);
protected:
    virtual bool sendData(const QnAbstractMediaDataPtr& data) override;
    virtual void prepareHttpClient(const nx::network::http::AsyncHttpClientPtr& httpClient) override;
    virtual bool isReadyForTransmission(
        nx::network::http::AsyncHttpClientPtr httpClient,
        bool isRetryAfterUnauthorizedResponse) const override;

    virtual nx::utils::Url transmissionUrl() const override;
    virtual std::chrono::milliseconds transmissionTimeout() const override;
    virtual nx::network::http::StringType contentType() const override;

private:
    AuthPolicy m_authPolicy = AuthPolicy::digestAndBasicAuth;
    nx::utils::Url m_url;
    nx::network::http::StringType m_contentType;
};

#endif // ENABLE_DATA_PROVIDERS
