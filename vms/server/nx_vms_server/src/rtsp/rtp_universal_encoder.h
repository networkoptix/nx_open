#pragma once

#include "rtsp/abstract_rtsp_encoder.h"
#include "transcoding/transcoder.h"
#include "transcoding/ffmpeg_transcoder.h"
#include "utils/common/byte_array.h"
#include <nx/streaming/rtp/rtcp.h>

class QnCommonModule;

class QnUniversalRtpEncoder: public AbstractRtspEncoder
{
public:
    struct Config: DecoderConfig
    {
        Config() = default;
        Config(const DecoderConfig& value): DecoderConfig(value) {}

        bool absoluteRtcpTimestamps = false;
        bool addOnvifHeaderExtension = false;
        bool useRealTimeOptimization = false;
        bool useMultipleSdpPayloadTypes = false;
    };

public:
    QnUniversalRtpEncoder(const Config& config, nx::metrics::Storage* metrics);

    bool open(
        QnConstAbstractMediaDataPtr mediaHigh,
        QnConstAbstractMediaDataPtr mediaLow,
        MediaQuality requiredQuality,
        AVCodecID transcodeToCodec,
        const QSize& videoSize,
        const QnLegacyTranscodingSettings& extraTranscodeParams);

    virtual QString getSdpMedia(bool isVideo, int trackId) override;

    virtual void setDataPacket(QnConstAbstractMediaDataPtr media) override;
    virtual bool getNextPacket(QnByteArray& sendBuffer) override;
    virtual void init() override;

private:
    void buildSdp(
        QnConstAbstractMediaDataPtr mediaHigh,
        QnConstAbstractMediaDataPtr mediaLow,
        bool transcoding,
        MediaQuality quality);

private:
    QnByteArray m_outputBuffer;
    QStringList m_sdpAttributes;
    Config m_config;
    bool m_isCurrentPacketSecondaryStream = false;
    bool m_useSecondaryPayloadType = false;
    MediaQuality m_requiredQuality = MEDIA_Quality_None;
    int m_outputPos = 0;
    int m_packetIndex = 0;
    QnFfmpegTranscoder m_transcoder;
    AVCodecID m_codec = AV_CODEC_ID_NONE;
    bool m_isVideo = false;
    int m_payloadType = 0;
    nx::streaming::rtp::RtcpSenderReporter m_rtcpReporter;
};

using QnUniversalRtpEncoderPtr = std::unique_ptr<QnUniversalRtpEncoder>;
