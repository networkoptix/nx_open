#pragma once

#include <core/dataconsumer/audio_data_transmitter.h>
#include <core/resource/security_cam_resource.h>
#include <nx/network/system_socket.h>
#include <transcoding/ffmpeg_audio_transcoder.h>
#include <nx/network/http/asynchttpclient.h>


class QnAxisAudioTransmitter : public QnAbstractAudioTransmitter
{
    typedef QnAbstractAudioTransmitter base_type;

    enum class TransmitterState
    {
        WaitingForConnection,
        ReadyForTransmission,
        Failed
    };

    Q_OBJECT
public:
    QnAxisAudioTransmitter(QnSecurityCamResource* res);
    virtual ~QnAxisAudioTransmitter();

    virtual bool processAudioData(const QnConstCompressedAudioDataPtr& data) override;

    virtual bool isCompatible(const QnAudioFormat& format) const override;
    virtual void setOutputFormat(const QnAudioFormat& format) override;
    virtual bool isInitialized() const override;

    virtual void prepare() override;

public slots:
    void at_requestHeadersHasBeenSent(
        nx_http::AsyncHttpClientPtr http,
        bool isRetryAfterUnauthorizedResponse);

    void at_httpDone(nx_http::AsyncHttpClientPtr http);

protected:
    virtual void pleaseStop() override;

private:
    bool startTransmission();
    nx_http::StringType mimeType() const;
    bool sendData(
        AbstractStreamSocket* socket,
        const char* buffer,
        size_t size);

    std::unique_ptr<AbstractStreamSocket> takeSocket(const nx_http::AsyncHttpClientPtr& httpClient);
private:
    mutable QnMutex m_mutex;
    QnSecurityCamResource* m_resource;
    std::unique_ptr<QnFfmpegAudioTranscoder> m_transcoder;
    QnAudioFormat m_outputFormat;
    std::unique_ptr<AbstractStreamSocket> m_socket;
    bool m_noAuth;

    TransmitterState m_state;
    QnWaitCondition m_wait;
    QElapsedTimer m_timer;
};
