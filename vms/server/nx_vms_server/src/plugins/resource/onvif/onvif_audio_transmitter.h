#pragma once

#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/streaming/rtsp_client.h>
#include <core/resource/camera_resource.h>
#include <transcoding/ffmpeg_audio_transcoder.h>
#include <core/dataconsumer/audio_data_transmitter.h>

namespace nx {
namespace vms::server {
namespace plugins {

class OnvifAudioTransmitter: public QnAbstractAudioTransmitter
{
    using base_type = QnAbstractAudioTransmitter;
public:
    OnvifAudioTransmitter(QnVirtualCameraResource* res);
    virtual ~OnvifAudioTransmitter();

    virtual bool processAudioData(const QnConstCompressedAudioDataPtr& data) override;
    virtual bool isCompatible(const QnAudioFormat& format) const override;
    virtual void setOutputFormat(const QnAudioFormat& /*format*/) override {}
    virtual void prepare() override;
    virtual void setBitrateKbps(int value) override;
protected:
    virtual void endOfRun() override;
private:
    bool isInitialized() const;
    bool sendData(const QnAbstractMediaDataPtr& audioData);
    bool openRtspConnection();
    void close();
private:
    QnVirtualCameraResource* m_resource;
    std::unique_ptr<QnFfmpegAudioTranscoder> m_transcoder;
    int m_bitrateKbps = 0;
    std::unique_ptr<QnRtspClient> m_rtspConnection;
    mutable QnMutex m_mutex;
    quint32 m_sequence = 0;
    QnRtspClient::SDPTrackInfo m_trackInfo;
    qint64 m_firstPts = AV_NOPTS_VALUE;
};

} // namespace plugins
} // namespace vms::server
} // namespace nx
