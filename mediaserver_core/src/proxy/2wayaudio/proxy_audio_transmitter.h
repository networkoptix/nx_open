#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <core/dataconsumer/audio_data_transmitter.h>
#include <utils/network/http/httpclient.h>
#include <utils/common/request_param.h>
#include <utils/network/abstract_socket.h>
#include <rtsp/rtsp_ffmpeg_encoder.h>

class QnProxyAudioTransmitter : public QnAbstractAudioTransmitter
{
    Q_OBJECT
public:

    typedef QnAbstractAudioTransmitter base_type;

    QnProxyAudioTransmitter(const QnResourcePtr& camera, const QnRequestParams &params);
    virtual ~QnProxyAudioTransmitter();

    virtual bool processAudioData(const QnConstCompressedAudioDataPtr& data) override;

    virtual bool isCompatible(const QnAudioFormat& format) const { return true; }
    virtual void setOutputFormat(const QnAudioFormat& /* format */) override {}
    virtual bool isInitialized() const override { return m_initialized; }

    static const QByteArray kFixedPostRequest;
protected:
    virtual void endOfRun() override;
private:
    QnResourcePtr m_camera;
    bool m_initialized;
    QSharedPointer<AbstractStreamSocket> m_socket;
    const QnRequestParams m_params;
    int m_sequence;
    std::unique_ptr<QnRtspFfmpegEncoder> m_serializer;
};

#endif // #ifdef ENABLE_DATA_PROVIDERS
