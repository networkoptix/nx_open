// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/rtp/parsers/rtp_parser.h>
#include <nx/rtp/parsers/rtp_stream_parser.h>
#include <nx/rtp/reordering_cache.h>
#include <nx/streaming/rtp/camera_time_helper.h>
#include <rtsp/srtp_encryptor.h>

#include "webrtc_tracks.h"
#include "webrtc_utils.h"

namespace nx::webrtc {

class NX_WEBRTC_API Demuxer
{
public:
    Demuxer(const Tracks* tracks);
    ~Demuxer();
    void setSdp(const std::string& sdp);
    bool processData(const char* data, size_t size);
    nx::Buffer getNextOutput(); //< Back to sender. Mostly it's RTCP NACKs.
    nx::Buffer getRtcpFirPacket(uint32_t videoSsrc); //< Back to sender. RTCP for emitting keyframe.
    QnAbstractMediaDataPtr getNextFrame(); //< From sender.
    void setSrtpEncryptionData(const rtsp::EncryptionData& data);
    void setResource(QnVirtualCameraResourcePtr resource) { m_resource = resource; }
    QnVirtualCameraResourcePtr resource() { return m_resource; }
    CodecParametersConstPtr audioCodecParameters() const { return m_audioCodecParameters; }

private:
    bool demux(const char* data, size_t size);
    bool processRtp(const nx::utils::ByteArray& array);
    bool processRtcp(const nx::utils::ByteArray& array);
    void encryptPacket(nx::Buffer& buffer);
    void processCameraTimeHelperEvent(nx::streaming::rtp::CameraTimeHelper::EventType event);

private:
    struct ParserContext
    {
        ParserContext(
            nx::rtp::RtpParser&& parser,
            uint32_t senderSsrc,
            std::unique_ptr<nx::streaming::rtp::CameraTimeHelper>&& timeHelper)
            :
            parser(std::move(parser)),
            senderSsrc(senderSsrc),
            timeHelper(std::move(timeHelper))
        {}
        nx::rtp::RtpParser parser;
        std::vector<uint8_t> buffer;
        nx::rtp::RtcpSenderReport senderReport;
        uint32_t senderSsrc = 0;
        std::unique_ptr<nx::streaming::rtp::CameraTimeHelper> timeHelper;
    };

private:
    const Tracks* m_tracks = nullptr;
    std::map<uint32_t, nx::rtp::ReorderingCache> m_reorderers; //< ssrc ->reorderer.
    std::map<uint32_t, ParserContext> m_parsers; //< ssrc -> parser.
    std::unique_ptr<rtsp::SrtpEncryptor> m_encryptor;
    std::deque<QnAbstractMediaDataPtr> m_frames;
    std::deque<nx::Buffer> m_feedbacks;
    QnVirtualCameraResourcePtr m_resource;
    nx::rtp::RtcpFirFeedback m_rtcpFir;
    CodecParametersConstPtr m_audioCodecParameters;
};

} // namespace nx::webrtc
