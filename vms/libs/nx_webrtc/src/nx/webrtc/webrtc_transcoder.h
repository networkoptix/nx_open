// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/scope_guard.h>
#include <nx/vms/api/data/webrtc_tracker_settings.h>
//#include <providers/live_stream_provider.h>
#include <rtsp/rtp_universal_encoder.h>
#include <rtsp/rtsp_ffmpeg_encoder.h>

#include "webrtc_utils.h"

class QnMediaServerModule;

namespace nx::webrtc {

class Session;

class NX_WEBRTC_API Transcoder
{
public:
    Transcoder(
        Session* session,
        nx::vms::api::WebRtcMethod method,
        nx::vms::api::WebRtcTrackerSettings::MseFormat mseFormat);

    bool createEncoders(AVCodecParameters* videoParameters, AVCodecParameters* audioParameters);
    void setSrtpEncryptionData(const rtsp::EncryptionData& data);
    rtsp::SrtpEncryptor* getEncryptor() const;
    nx::vms::api::WebRtcMethod method() const { return m_method; }
    bool isVideoCodecSupported(AVCodecID codecId);
    bool isAudioCodecSupported(AVCodecID codecId);
    const std::string& mimeType() const { return m_mimeType; }

    QnUniversalRtpEncoder* videoEncoder() const;
    QnUniversalRtpEncoder* audioEncoder() const;
    QnUniversalRtpEncoder* encoderBySsrc(uint32_t ssrc) const;

    // API for Consumer.
    void setDataPacket(QnConstAbstractMediaDataPtr media);
    bool getNextPacket(QnAbstractMediaData::DataType type, nx::utils::ByteArray& buffer);
    bool isEof(QnAbstractMediaData::DataType type) const;
    FfmpegMuxer::PacketTimestamp getLastTimestamps() const;
    int64_t getLastTimestampMs() const { return m_lastTimestampMs; } //< Required for reconnection.

private:
    QnUniversalRtpEncoderPtr createRtpEncoder(
        AVCodecParameters* codecParameters,
        uint32_t ssrc,
        const std::string& cname);

    bool createRtpEncoders(AVCodecParameters* videoParameters, AVCodecParameters* audioParameters);
    bool createMseEncoder(AVCodecParameters* videoParameters, AVCodecParameters* audioParameters);
    bool checkForMseEof(QnConstAbstractMediaDataPtr media);
    bool constructMimeType();
    QnUniversalRtpEncoder* getRtpEncoder(QnAbstractMediaData::DataType type) const;

private:
    Session* m_session;

    // SRTP transcoding.
    QnUniversalRtpEncoderPtr m_videoEncoder;
    QnUniversalRtpEncoderPtr m_audioEncoder;

    // MSE transcoding.
    std::unique_ptr<FfmpegMuxer> m_mseMuxer;
    std::queue<nx::utils::ByteArray> m_mseBuffers;
    bool m_eof = false;
    // Count of video and audio frames received for last gop.
    int m_videoPacketsCount = 0;
    int m_audioPacketsCount = 0;
    std::string m_mimeType;
    nx::vms::api::WebRtcMethod m_method = nx::vms::api::WebRtcMethod::srtp;
    nx::vms::api::WebRtcTrackerSettings::MseFormat m_mseFormat;
    int64_t m_lastTimestampMs = 0;
};

} // namespace nx::webrtc
