#pragma once

#include <core/dataconsumer/audio_data_transmitter.h>
#include <core/resource/security_cam_resource.h>
#include <utils/network/system_socket.h>
#include <transcoding/ffmpeg_audio_transcoder.h>

class QnAxisAudioTransmitter : public QnAbstractAudioTransmitter
{
    typedef QnAbstractAudioTransmitter base_type;

    Q_OBJECT
public:
    QnAxisAudioTransmitter(QnSecurityCamResource* res);
    virtual ~QnAxisAudioTransmitter();

    virtual bool processAudioData(QnConstAbstractMediaDataPtr &data) override;

    virtual bool isCompatible(const QnOutputAudioFormat& format) const override;
    virtual void setOutputFormat(const QnOutputAudioFormat& format) override;
    virtual bool isInitialized() const override;

    virtual void prepare() override;
protected:
    virtual void pleaseStop() override;
private:
    bool establishConnection();
    SocketAddress getSocketAddress() const;
    QByteArray buildTransmitRequest() const;
    QString getAudioMimeType() const;
    int sendData(const char* data, int dataSize);

private:
    mutable QnMutex m_mutex;
    QnSecurityCamResource* m_resource;
    std::unique_ptr<QnFfmpegAudioTranscoder> m_transcoder;
    std::atomic<bool> m_connectionEstablished;
    bool m_canProcessData;
    std::unique_ptr<TCPSocket> m_socket;
    QnOutputAudioFormat m_outputFormat;
};
