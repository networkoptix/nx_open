// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/rtp/rtcp.h>
#include <nx/rtp/rtcp_nack_responder.h>
#include <nx/utils/byte_array.h>

#include "rtsp/abstract_rtsp_encoder.h"
#include "srtp_encryptor.h"
#include "transcoding/ffmpeg_transcoder.h"
#include "transcoding/transcoder.h"

class QnCommonModule;

class NX_VMS_COMMON_API QnUniversalRtpEncoder: public AbstractRtspEncoder
{
public:
    struct Config
    {
        Config() = default;

        bool absoluteRtcpTimestamps = false;
        bool addOnvifHeaderExtension = false;
        bool useRealTimeOptimization = false;
        bool useMultipleSdpPayloadTypes = false;
        bool useRtcpNack = false;
        bool webRtcMode = false;
        QnFfmpegTranscoder::Config transcoderConfig;
    };

public:
    QnUniversalRtpEncoder(const Config& config, nx::metric::Storage* metrics);

    bool open(
        QnConstAbstractMediaDataPtr mediaHigh,
        QnConstAbstractMediaDataPtr mediaLow,
        MediaQuality requiredQuality,
        AVCodecID transcodeToCodec,
        const QSize& videoSize,
        const QnLegacyTranscodingSettings& extraTranscodeParams);

    virtual QString getSdpMedia(bool isVideo, int trackId, int port = 0) override;

    virtual void setDataPacket(QnConstAbstractMediaDataPtr media) override;
    virtual bool getNextPacket(nx::utils::ByteArray& sendBuffer) override;
    virtual void init() override;
    virtual bool isEof() const override;
    virtual void setMtu(int mtu) override;

    void setRtcpPacket(uint8_t* data, int size);
    bool getNextNackPacket(nx::utils::ByteArray& sendBuffer);
    void setSrtpEncryptionData(const nx::vms::server::rtsp::EncryptionData& data);
    nx::vms::server::rtsp::SrtpEncryptor* encryptor() const;
    bool isTranscodingEnabled() const;
    QnFfmpegTranscoder::PacketTimestamp getLastTimestamps() const;
    void setSeeking();
    int payloadType() const;
private:
    QSize getTargetSize(QnConstAbstractMediaDataPtr media, QSize targetSize);
    void buildSdp(
        QnConstAbstractMediaDataPtr mediaHigh,
        QnConstAbstractMediaDataPtr mediaLow,
        bool transcoding,
        MediaQuality quality);

private:
    nx::utils::ByteArray m_outputBuffer;
    QStringList m_sdpAttributes;
    Config m_config;
    bool m_isCurrentPacketSecondaryStream = false;
    bool m_useSecondaryPayloadType = false;
    bool m_eof = false;
    bool m_transcodingEnabled = false;
    MediaQuality m_requiredQuality = MEDIA_Quality_None;
    int m_outputPos = 0;
    int m_packetIndex = 0;
    QnFfmpegTranscoder m_transcoder;
    AVCodecID m_codec = AV_CODEC_ID_NONE;
    AVCodecID m_sourceCodec = AV_CODEC_ID_NONE;
    bool m_isVideo = false;
    int m_payloadType = 0;
    nx::rtp::RtcpSenderReporter m_rtcpReporter;
    std::unique_ptr<nx::vms::server::rtsp::SrtpEncryptor> m_encryptor;
    std::unique_ptr<nx::rtp::RtcpNackResponder> m_rtcpNackResponder;
};

using QnUniversalRtpEncoderPtr = std::unique_ptr<QnUniversalRtpEncoder>;
