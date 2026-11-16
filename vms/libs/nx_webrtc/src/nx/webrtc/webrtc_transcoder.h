// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/scope_guard.h>
#include <nx/vms/api/data/webrtc_tracker_settings.h>
#include <rtsp/rtp_universal_encoder.h>
#include <rtsp/rtsp_ffmpeg_encoder.h>

#include "webrtc_utils.h"

class QnMediaServerModule;

namespace nx::webrtc {

class Session;

/**
 * The payload type a codec is offered under on a WebRTC m-line, or nullopt for a codec that has
 * no number of its own. An m-line offers every codec its Device may switch to at once, so each
 * of them needs a payload type the others don't use -- getPayloadType() gives H.264 and H.265
 * the same one -- and the numbers have to be unique across the whole BUNDLE group (RFC 8843
 * section 9.1), which the static payload types of the audio codecs satisfy on their own.
 */
NX_WEBRTC_API std::optional<int> rtpPayloadType(AVCodecID codecId);

/**
 * The rtpmap and fmtp lines offering `codecId` on an m-line that does not stream it (yet), so
 * without anything that would have to come from the stream itself. Empty for a codec that is
 * not offered this way.
 */
NX_WEBRTC_API QStringList alternativeCodecSdp(AVCodecID codecId, int payloadType);

class NX_WEBRTC_API Transcoder
{
public:
    Transcoder(
        Session* session,
        nx::vms::api::WebRtcMethod method,
        nx::vms::api::WebRtcTrackerSettings::MseFormat mseFormat);

    bool createEncoders(
        nx::Uuid deviceId,
        AVCodecParameters* videoParameters,
        AVCodecParameters* audioParameters);

    // Lets go of everything kept for `deviceId`. Its encoders outlive it otherwise, for the
    // whole session, and getEncryptor() and getLastTimestamps() go on picking them.
    void removeEncoders(nx::Uuid deviceId);

    // Lets go of the encoder of a single track of `deviceId`, leaving the other one, which the
    // Device still streams with. See removeEncoders().
    void removeEncoder(nx::Uuid deviceId, QnAbstractMediaData::DataType videoOrAudio);
    void setSrtpEncryptionData(const rtsp::EncryptionData& data);
    rtsp::SrtpDecryptor* getDecryptor() const;
    nx::vms::api::WebRtcMethod method() const { return m_method; }
    bool isVideoCodecSupported(AVCodecID codecId);
    bool isAudioCodecSupported(AVCodecID codecId);
    const std::string& mimeType() const { return m_mimeType; }

    QnUniversalRtpEncoder* videoEncoder(nx::Uuid deviceId) const;
    QnUniversalRtpEncoder* audioEncoder(nx::Uuid deviceId) const;
    QnUniversalRtpEncoder* encoderBySsrc(uint32_t ssrc) const;

    // API for Consumer.
    QnUniversalRtpEncoder* setDataPacket(
        nx::Uuid deviceId,
        QnConstAbstractMediaDataPtr media);
    bool getNextPacket(
        QnUniversalRtpEncoder* rtpEncoder,
        nx::utils::ByteArray& buffer);
    bool isEof(nx::Uuid deviceId, QnUniversalRtpEncoder* rtpEncoder) const;

    /**
     * Takes the RTP state of one encoder of `deviceId` so that the encoder replacing it can go
     * on streaming to the same receiver, and gives it to the new one once it is built. Only for
     * a rebuild that keeps the SSRC of the track, see Session::switchCodec().
     * NOTE: between the two calls the Device must packetize nothing: takeContinuity() leaves
     * the old encoder without its SRTP context.
     */
    QnUniversalRtpEncoder::Continuity takeContinuity(
        nx::Uuid deviceId, QnAbstractMediaData::DataType videoOrAudio);
    void restoreContinuity(nx::Uuid deviceId,
        QnAbstractMediaData::DataType videoOrAudio,
        QnUniversalRtpEncoder::Continuity continuity);
    FfmpegMuxer::PacketTimestamp getLastTimestamps() const;
    int64_t getLastTimestampMs() const { return m_lastTimestampMs; } //< Required for reconnection.

private:
    QnUniversalRtpEncoderPtr createRtpEncoder(
        AVCodecParameters* codecParameters,
        uint32_t ssrc,
        const std::string& cname,
        int mid);

    bool createRtpEncoders(
        nx::Uuid deviceId,
        AVCodecParameters* videoParameters,
        AVCodecParameters* audioParameters);
    bool createMseEncoder(AVCodecParameters* videoParameters, AVCodecParameters* audioParameters);
    bool checkForMseEof(QnConstAbstractMediaDataPtr media);
    bool constructMimeType();
    QnUniversalRtpEncoder* getRtpEncoder(
        nx::Uuid deviceId,
        QnAbstractMediaData::DataType type) const;

private:
    Session* m_session;

    // SRTP transcoding.
    std::map<nx::Uuid, QnUniversalRtpEncoderPtr> m_videoEncoders;
    std::map<nx::Uuid, QnUniversalRtpEncoderPtr> m_audioEncoders;

    // MSE transcoding.
    std::unique_ptr<FfmpegMuxer> m_mseMuxer;
    std::queue<nx::utils::ByteArray> m_mseBuffers;
    std::set<nx::Uuid> m_eof;
    // Count of video and audio frames received for last gop.
    int m_videoPacketsCount = 0;
    int m_audioPacketsCount = 0;
    std::string m_mimeType;
    nx::vms::api::WebRtcMethod m_method = nx::vms::api::WebRtcMethod::srtp;
    nx::vms::api::WebRtcTrackerSettings::MseFormat m_mseFormat;
    int64_t m_lastTimestampMs = 0;
    std::optional<rtsp::EncryptionData> m_encryptionData;
    std::unique_ptr<rtsp::SrtpDecryptor> m_decryptor;
};

} // namespace nx::webrtc
