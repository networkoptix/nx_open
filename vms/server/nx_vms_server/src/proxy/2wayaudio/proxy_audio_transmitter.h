#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <core/dataconsumer/audio_data_transmitter.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <utils/common/request_param.h>
#include <nx/network/socket.h>
#include <rtsp/rtsp_ffmpeg_encoder.h>
#include <common/common_module_aware.h>

class QnProxyAudioTransmitter: public QnAbstractAudioTransmitter, public /*mixin*/ QnCommonModuleAware
{
    Q_OBJECT
public:

    typedef QnAbstractAudioTransmitter base_type;

    QnProxyAudioTransmitter(
        QnCommonModule* commonModule,
        const QnResourcePtr& camera,
        const QnRequestParams &params);
    virtual ~QnProxyAudioTransmitter();

    virtual bool processAudioData(const QnConstCompressedAudioDataPtr& data) override;

    virtual bool isCompatible(const QnAudioFormat& /* format */) const { return true; }
    virtual void setOutputFormat(const QnAudioFormat& /* format */) override {}

protected:
    virtual void endOfRun() override;
private:
    bool isInitialized() const { return m_initialized; }
private:
    QnResourcePtr m_camera;
    bool m_initialized;
    std::unique_ptr<nx::network::AbstractStreamSocket> m_socket;
    const QnRequestParams m_params;
    std::unique_ptr<QnRtspFfmpegEncoder> m_serializer;
};

#endif // #ifdef ENABLE_DATA_PROVIDERS
