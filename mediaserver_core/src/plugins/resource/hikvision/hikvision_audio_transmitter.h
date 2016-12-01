#pragma once

#include <core/dataconsumer/base_http_audio_transmitter.h>
#include <core/resource/security_cam_resource.h>
#include <nx/network/socket.h>
#include <transcoding/ffmpeg_audio_transcoder.h>
#include <nx/network/http/asynchttpclient.h>

class HikvisionAudioTransmitter: public BaseHttpAudioTransmitter
{
    using base_type = BaseHttpAudioTransmitter;

    struct ChannelStatusResponse
    {
        QString id;
        bool enabled = false;
        QString audioCompression;
        QString audioInputType;
        int speakerVolume = 0;
        bool noiseReduce = false;
    };

    struct OpenChannelResponse
    {
        QString sessionId;
    };

    struct CommonResponse
    {
        QString statusCode;
        QString statusString;
        QString subStatusCode;
    };

public:
    HikvisionAudioTransmitter(QnSecurityCamResource* resource);
    virtual bool isCompatible(const QnAudioFormat& format) const override;

    void setChannelId(QString channelId);

protected:
    virtual bool sendData(const QnAbstractMediaDataPtr& data) override;
    virtual void prepareHttpClient(nx_http::AsyncHttpClientPtr httpClient) override;
    virtual bool isReadyForTransmission(
        nx_http::AsyncHttpClientPtr httpClient,
        bool isRetryAfterUnauthorizedResponse) const override;

    virtual QUrl transmissionUrl() const override;
    virtual std::chrono::milliseconds transmissionTimeout() const override;
    virtual nx_http::StringType contentType() const override;

private:
    bool openChannelIfNeeded();
    bool closeChannel();

    boost::optional<ChannelStatusResponse> parseChannelStatusResponse(
        nx_http::StringType message) const;
    boost::optional<OpenChannelResponse> parseOpenChannelResponse(
        nx_http::StringType message) const;
    boost::optional<CommonResponse> parseCommonResponse(
        nx_http::StringType message) const;

private:
    QString m_channelId;
};