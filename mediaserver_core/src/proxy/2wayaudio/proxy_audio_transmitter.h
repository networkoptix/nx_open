#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <core/dataconsumer/audio_data_transmitter.h>
#include <nx/network/http/asynchttpclient.h>
#include <utils/common/request_param.h>
#include <nx/network/socket.h>
#include <rtsp/rtsp_ffmpeg_encoder.h>

class QnProxyAudioTransmitter : public QnAbstractAudioTransmitter
{
    Q_OBJECT
public:

    typedef QnAbstractAudioTransmitter base_type;

    QnProxyAudioTransmitter(const QnResourcePtr& camera, const QnRequestParams &params);
    virtual ~QnProxyAudioTransmitter();

    virtual bool processAudioData(const QnConstCompressedAudioDataPtr& data) override;

    virtual bool isCompatible(const QnAudioFormat& /* format */) const { return true; }
    virtual void setOutputFormat(const QnAudioFormat& /* format */) override {}

    static const QByteArray kFixedPostRequest;
protected:
    virtual void endOfRun() override;
private:
    bool isInitialized() const { return m_initialized; }
private:
    QnResourcePtr m_camera;
    bool m_initialized;
    std::unique_ptr<AbstractStreamSocket> m_socket;
    const QnRequestParams m_params;
    int m_sequence;
    std::unique_ptr<QnRtspFfmpegEncoder> m_serializer;
};

#endif // #ifdef ENABLE_DATA_PROVIDERS
