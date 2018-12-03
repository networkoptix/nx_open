#pragma once

#include <core/dataconsumer/base_http_audio_transmitter.h>
#include <core/resource/security_cam_resource.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/http/http_client.h>

namespace nx {
namespace vms::server {
namespace plugins {

class HikvisionAudioTransmitter: public BaseHttpAudioTransmitter
{
    using base_type = BaseHttpAudioTransmitter;

public:
    HikvisionAudioTransmitter(QnSecurityCamResource* resource);
    virtual ~HikvisionAudioTransmitter();
    virtual bool isCompatible(const QnAudioFormat& format) const override;
    void setChannelId(QString channelId);

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
    bool openChannelIfNeeded();
    bool closeChannel();

    std::unique_ptr<nx::network::http::HttpClient> createHttpHelper();

private:
    QString m_channelId;
    bool m_noAuth = false;
};

} // namespace plugins
} // namespace vms::server
} // namespace nx
