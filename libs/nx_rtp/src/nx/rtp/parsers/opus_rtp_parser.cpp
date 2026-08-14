// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "opus_rtp_parser.h"

#include <algorithm>
#include <array>
#include <string_view>

#include <nx/media/audio_data_packet.h>
#include <nx/media/codec_parameters.h>
#include <nx/rtp/rtp.h>
#include <nx/utils/log/log.h>

namespace nx::rtp {

namespace {

using OpusHead = std::array<uint8_t, 19>;

// OpusHead identification header, RFC 7845, 5.1. The Matroska and MP4 muxers require it as
// extradata, but it is not part of the RTP stream, so it has to be built from the SDP
OpusHead makeOpusHead(int channels, int inputSampleRate)
{
    constexpr std::string_view kSignature = "OpusHead";

    OpusHead head{};
    std::copy(kSignature.begin(), kSignature.end(), head.begin());
    head[8] = 1; //< Version.
    head[9] = (uint8_t) channels;
    // head[10..11] pre-skip: stays 0, RTP carries no encoder delay to trim.
    // head[12..15] input sample rate, little endian. Informational only per RFC 7845.
    head[12] = (uint8_t) inputSampleRate;
    head[13] = (uint8_t) (inputSampleRate >> 8);
    head[14] = (uint8_t) (inputSampleRate >> 16);
    head[15] = (uint8_t) (inputSampleRate >> 24);
    // head[16..17] output gain: stays 0, no replay gain to apply.
    // head[18] channel mapping family: stays 0, meaning plain mono or stereo.
    return head;
}

// RFC 7587, 7.1: sprop-stereo can only be 0 (strictly mono, default) or 1 (one or more channels)
int channelsFromSdp(const Sdp::Media& sdp)
{
    return std::ranges::contains(sdp.fmtp.params, QString("sprop-stereo=1")) ? 2 : 1;
}

} // namespace

void OpusParser::setSdpInfo(const Sdp::Media& sdp)
{
    if (sdp.rtpmap.clockRate > 0)
    {
        if (sdp.rtpmap.clockRate != kClockRate)
        {
            NX_DEBUG(this,
                "Opus clock rate is %1 instead of the %2 required by RFC 7587",
                sdp.rtpmap.clockRate,
                kClockRate);
        }
        StreamParser::setFrequency(sdp.rtpmap.clockRate);
    }
    m_channels = channelsFromSdp(sdp);

    auto context = std::make_shared<CodecParameters>();
    const auto codecParams = context->getAvCodecParameters();
    codecParams->codec_type = AVMEDIA_TYPE_AUDIO;
    codecParams->codec_id = AV_CODEC_ID_OPUS;
    av_channel_layout_default(&codecParams->ch_layout, m_channels);
    codecParams->sample_rate = StreamParser::getFrequency();
    codecParams->format = AV_SAMPLE_FMT_FLTP;

    const auto opusHead = makeOpusHead(m_channels, StreamParser::getFrequency());
    context->setExtradata(opusHead.data(), static_cast<int>(opusHead.size()));
    m_context = context;
}

Result OpusParser::processData(const RtpHeader& rtpHeader,
    uint8_t* rtpBufferBase,
    int bufferOffset,
    int bufferSize,
    bool& gotData)
{
    gotData = false;
    if (bufferSize <= 0)
        return {false, "Empty Opus packet"};

    // RFC 7587, section 4: one Opus packet per RTP packet and no payload header, so the whole
    // payload is the compressed frame.
    auto audioData =
        QnWritableCompressedAudioDataPtr(new QnWritableCompressedAudioData(bufferSize));
    audioData->compressionType = AV_CODEC_ID_OPUS;
    audioData->context = m_context;
    audioData->timestamp = rtpHeader.getTimestamp();
    audioData->m_data.write(
        reinterpret_cast<const char*>(rtpBufferBase + bufferOffset), bufferSize);
    m_audioData.push_back(audioData);
    gotData = true;
    return {true};
}

} // namespace nx::rtp
