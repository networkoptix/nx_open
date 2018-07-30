#pragma once

#include "rtsp/rtsp_encoder.h"
#include "transcoding/transcoder.h"
#include "transcoding/ffmpeg_transcoder.h"
#include "utils/common/byte_array.h"
#include <nx/streaming/rtp/rtcp.h>

class QnCommonModule;

class QnUniversalRtpEncoder: public QnRtspEncoder
{
public:
    using Ptr = QSharedPointer<QnUniversalRtpEncoder>;
    struct Config
    {
        bool absoluteRtcpTimestamps = false;
        bool addOnvifHeaderExtension = false;
        bool useRealTimeOptimization = false;
        bool useMultipleSdpPayloadTypes = false;
    };

public:
    QnUniversalRtpEncoder(const Config& config, QnCommonModule* commonModule);

    bool open(
        QnConstAbstractMediaDataPtr mediaHigh,
        QnConstAbstractMediaDataPtr mediaLow,
        MediaQuality requiredQuality,
        AVCodecID transcodeToCodec,
        const QSize& videoSize,
        const QnLegacyTranscodingSettings& extraTranscodeParams);

    virtual QByteArray getAdditionSDP() override;

    virtual void setDataPacket(QnConstAbstractMediaDataPtr media) override;
    virtual bool getNextPacket(QnByteArray& sendBuffer) override;
    virtual void init() override;

    virtual quint32 getSSRC() override;
    virtual bool getRtpMarker() override;
    virtual quint32 getFrequency() override;

    virtual quint8 getPayloadtype() override;
    virtual QString getPayloadTypeStr() override;
    virtual QString getName() override;

    virtual bool isRtpHeaderExists() const override { return true; }
    void setUseRealTimeOptimization(bool value);

private:
    void buildSdp(
        QnConstAbstractMediaDataPtr mediaHigh,
        QnConstAbstractMediaDataPtr mediaLow,
        bool transcoding,
        MediaQuality quality);

private:
    QnByteArray m_outputBuffer;
    QByteArray m_sdp;
    Config m_config;
    bool m_isCurrentPacketSecondaryStream = false;
    bool m_useSecondaryPayloadType = false;
    MediaQuality m_requiredQuality;
    int m_outputPos = 0;
    int m_packetIndex = 0;
    QnFfmpegTranscoder m_transcoder;
    AVCodecID m_codec;
    bool m_isVideo;
    nx::streaming::rtp::RtcpSenderReporter m_rtcpReporter;
};
