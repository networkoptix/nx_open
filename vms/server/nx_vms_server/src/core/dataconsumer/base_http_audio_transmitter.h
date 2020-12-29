#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QElapsedTimer>

#include "audio_data_transmitter.h"
#include <nx/network/deprecated/asynchttpclient.h>
#include <transcoding/ffmpeg_audio_transcoder.h>

class BaseHttpAudioTransmitter: public QnAbstractAudioTransmitter
{
    using  base_type = QnAbstractAudioTransmitter;

protected:
    enum class TransmitterState
    {
        WaitingForConnection,
        ReadyForTransmission,
        Failed
    };

public:
    BaseHttpAudioTransmitter(QnSecurityCamResource* res);
    virtual ~BaseHttpAudioTransmitter();
    virtual void setOutputFormat(const QnAudioFormat& format) override;
    virtual void setBitrateKbps(int value) override;
    virtual void setAudioUploadHttpMethod(nx::network::http::StringType method);
    virtual void prepare() override;
    virtual bool processAudioData(const QnConstCompressedAudioDataPtr& audioData) override;

protected:
    virtual bool sendData(const QnAbstractMediaDataPtr& data) = 0;
    virtual void prepareHttpClient(const nx::network::http::AsyncHttpClientPtr& httpClient) = 0;
    virtual bool isReadyForTransmission(
        nx::network::http::AsyncHttpClientPtr httpClient,
        bool isRetryAfterUnauthorizedResponse) const = 0;

    virtual nx::utils::Url transmissionUrl() const = 0;
    virtual std::chrono::milliseconds transmissionTimeout() const = 0;
    virtual nx::network::http::StringType contentType() const = 0;

    virtual void pleaseStop() override;
    virtual void endOfRun() override;
protected:
    bool sendBuffer(nx::network::AbstractStreamSocket* socket, const char* buffer, size_t size);
    std::unique_ptr<nx::network::AbstractStreamSocket> takeSocket(
        const nx::network::http::AsyncHttpClientPtr& httpClient) const;

    bool startTransmission();

private:
    bool isInitialized() const;
    void at_requestHeadersHasBeenSent(
        nx::network::http::AsyncHttpClientPtr httpClient,
        bool isRetryAfterUnauthorizedResponse);

    void at_httpDone(nx::network::http::AsyncHttpClientPtr httpClient);


protected:
    QnSecurityCamResource* m_resource;
    TransmitterState m_state;
    QnAudioFormat m_outputFormat;
    int m_bitrateKbps;
    std::unique_ptr<QnFfmpegAudioTranscoder> m_transcoder;
    std::unique_ptr<nx::network::AbstractStreamSocket> m_socket;
    QElapsedTimer m_timer;
    nx::network::http::StringType m_uploadMethod;
    mutable QnMutex m_mutex;
    mutable QnWaitCondition m_wait;
};

#endif //ENABLE_DATA_PROVIDERS
