#pragma once

#include <core/dataconsumer/base_http_audio_transmitter.h>
#include <core/resource/security_cam_resource.h>
#include <nx/network/deprecated/asynchttpclient.h>

class QnAxisAudioTransmitter : public BaseHttpAudioTransmitter
{
    using base_type = BaseHttpAudioTransmitter;

    Q_OBJECT
public:
    QnAxisAudioTransmitter(QnSecurityCamResource* res);
    virtual bool isCompatible(const QnAudioFormat& format) const override;

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
};
