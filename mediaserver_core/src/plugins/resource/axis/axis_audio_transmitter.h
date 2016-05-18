#pragma once

#include <core/dataconsumer/audio_data_transmitter.h>
#include <core/resource/security_cam_resource.h>
#include <utils/network/system_socket.h>
#include <transcoding/ffmpeg_audio_transcoder.h>
#include <utils/network/http/asynchttpclient.h>


typedef std::function<void(SystemError::ErrorCode, nx_http::BufferType)>
        ReadCallbackType;

class QnAxisAudioMsgBodySource : public nx_http::AbstractMsgBodySource
{

public:
    QnAxisAudioMsgBodySource();
    virtual nx_http::StringType mimeType() const override;
    virtual boost::optional<uint64_t> contentLength() const override;

    virtual void readAsync(ReadCallbackType completionHandler) override;

    void setOutputFormat(const QnAudioFormat& format);
    bool isWaitingForData() const;
    void putData(const nx_http::BufferType& data);

    ReadCallbackType callback() const;

private:
    mutable QnMutex m_mutex;
    QnAudioFormat m_outputFormat;
    ReadCallbackType m_callback;
    mutable bool m_waitingForNextData;
};





class QnAxisAudioTransmitter : public QnAbstractAudioTransmitter
{
    typedef QnAbstractAudioTransmitter base_type;

    Q_OBJECT
public:
    QnAxisAudioTransmitter(QnSecurityCamResource* res);
    virtual ~QnAxisAudioTransmitter();

    virtual bool processAudioData(QnConstAbstractMediaDataPtr &data) override;

    virtual bool isCompatible(const QnAudioFormat& format) const override;
    virtual void setOutputFormat(const QnAudioFormat& format) override;
    virtual bool isInitialized() const override;

    virtual void prepare() override;

protected:
    virtual void pleaseStop() override;

private:
    bool startTransmission();

private:
    mutable QnMutex m_mutex;
    QnSecurityCamResource* m_resource;
    std::unique_ptr<QnFfmpegAudioTranscoder> m_transcoder;
    QnAudioFormat m_outputFormat;

    nx_http::BufferType m_transcodedBuffer;
    std::shared_ptr<QnAxisAudioMsgBodySource> m_bodySource;
    nx_http::AsyncHttpClientPtr m_httpClient;

    bool m_dataTransmissionStarted;
};
