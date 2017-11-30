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
    QnBasicAudioTransmitter(QnSecurityCamResource* res);
    void setTransmissionUrl(const nx::utils::Url &url);
    void setContentType(const nx_http::StringType& contentType);
    void setNoAuth(bool value);
protected:
    virtual bool sendData(const QnAbstractMediaDataPtr& data) override;
    virtual void prepareHttpClient(const nx_http::AsyncHttpClientPtr& httpClient) override;
    virtual bool isReadyForTransmission(
        nx_http::AsyncHttpClientPtr httpClient,
        bool isRetryAfterUnauthorizedResponse) const override;

    virtual nx::utils::Url transmissionUrl() const override;
    virtual std::chrono::milliseconds transmissionTimeout() const override;
    virtual nx_http::StringType contentType() const override;

private:
    bool m_noAuth;
    nx::utils::Url m_url;
    nx_http::StringType m_contentType;
};

#endif // ENABLE_DATA_PROVIDERS
