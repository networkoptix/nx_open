// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <vector>

#include <nx/rtp/rtcp.h>
#include <nx/rtp/rtcp_nack_responder.h>
#include <nx/utils/byte_array.h>

#include <transcoding/ffmpeg_transcoder.h>
#include "rtsp/abstract_rtsp_encoder.h"
#include "srtp_encryptor.h"

class NX_VMS_COMMON_API QnUniversalRtpEncoder: public AbstractRtspEncoder
{
public:
    struct Config
    {
        Config() = default;

        bool absoluteRtcpTimestamps = false;
        bool addOnvifHeaderExtension = false;
        bool useMultipleSdpPayloadTypes = false;
        bool useRtcpNack = false;
        bool webRtcMode = false;
        // The payload type to use instead of the one the codec implies. A WebRTC video m-line
        // offers H.264 and H.265 at once, which works only if each of them has a payload type
        // of its own, while getPayloadType() gives both the same one.
        std::optional<int> payloadType;
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
        const nx::core::transcoding::Settings& extraTranscodeParams);

    bool open(const AVCodecParameters* codecParameters);

    virtual QString getSdpMedia(bool isVideo, int trackId, int port = 0, bool ssl = false) override;

    virtual void setDataPacket(QnConstAbstractMediaDataPtr media) override;
    virtual bool getNextPacket(nx::utils::ByteArray& sendBuffer) override;
    virtual void init() override;
    virtual bool isEof() const override;
    virtual void setMtu(int mtu) override;

    void setRtcpPacket(uint8_t* data, int size);
    bool getNextNackPacket(nx::utils::ByteArray& sendBuffer);
    void setSrtpEncryptionData(const nx::rtsp::EncryptionData& data);
    nx::rtsp::SrtpEncryptor* encryptor() const;
    FfmpegMuxer::PacketTimestamp getLastTimestamps() const;
    int payloadType() const;

    QStringList sdpAttributes() const;
    AVCodecID sourceCodec() const { return m_sourceCodec; }

    /**
     * Puts `payloadTypes` on the m-line next to the one this encoder streams, and `sdpAttributes`
     * (their rtpmap and fmtp lines) after its own. They describe codecs that nothing streams
     * yet: a WebRTC m-line offers every codec its Device may switch to, so that the switch
     * itself needs no new SDP. See nx::webrtc::alternativeCodecSdp().
     */
    void setAlternativeCodecs(std::vector<int> payloadTypes, QStringList sdpAttributes);
    const std::vector<int>& alternativePayloadTypes() const { return m_altPayloadTypes; }

    /**
     * RTP state that has to outlive this encoder when another one takes over the same stream,
     * as happens when a Device changes codec and the encoder is rebuilt around the new one
     * without touching the SSRC. The receiver reads that as one continuous RTP stream, so the
     * numbering may not restart; and SRTP must never encrypt two packets with the same
     * (SSRC, index) under one key, which a fresh libsrtp session on an unchanged SSRC would do.
     * See nx::webrtc::Session::switchCodec().
     */
    struct Continuity
    {
        uint16_t sequence = 0;
        uint32_t rtpTimestampBase = 0;
        std::unique_ptr<nx::rtsp::SrtpEncryptor> encryptor;
        nx::rtp::RtcpSenderReporter rtcpReporter;
        // Holds the packets sent so far, by sequence number, so that the ones sent before the
        // switch can still be retransmitted when the receiver asks for them.
        std::unique_ptr<nx::rtp::RtcpNackResponder> nackResponder;
    };

    // NOTE: leaves this encoder without an SRTP context, so it must not packetize anything
    // afterwards: it is meant for an encoder that is about to be replaced.
    Continuity takeContinuity();
    void restoreContinuity(Continuity continuity);

    void setMid(const std::string& value) { m_mid = value; }
    std::string mid() const { return m_mid; }
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
    AVCodecID m_sourceCodec = AV_CODEC_ID_NONE;
    bool m_isVideo = false;
    int m_payloadType = 0;
    nx::rtp::RtcpSenderReporter m_rtcpReporter;
    std::unique_ptr<nx::rtsp::SrtpEncryptor> m_encryptor;
    std::unique_ptr<nx::rtp::RtcpNackResponder> m_rtcpNackResponder;
    std::string m_mid;
    // Seeded from the first packet the muxer produces and the encoder's own from there on, so
    // that an encoder taking over a stream can go on numbering it. See Continuity.
    uint16_t m_sequence = 0;
    bool m_sequenceInitialized = false;
    // Added to every RTP timestamp the muxer produces, which it numbers from zero.
    uint32_t m_rtpTimestampBase = 0;
    uint32_t m_lastRtpTimestamp = 0;
    std::vector<int> m_altPayloadTypes;
    QStringList m_altSdpAttributes;
};

using QnUniversalRtpEncoderPtr = std::unique_ptr<QnUniversalRtpEncoder>;
