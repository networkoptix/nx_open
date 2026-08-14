// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>

#include <nx/rtp/parsers/rtp_stream_parser.h>

namespace nx::rtp {

/**
 * Opus RTP payload format, RFC 7587. Exactly one Opus packet is carried per RTP packet and there
 * is no payload header, so the payload is passed through to the container as is.
 *
 * Two things are not carried in the RTP stream and are reconstructed here:
 * - The rtpmap of an Opus track always declares 48000 Hz and 2 channels no matter what the sender
 *   actually encodes (RFC 7587, section 7). The `sprop-stereo` fmtp parameter is what tells us
 *   whether the sender may produce stereo.
 * - Containers need the OpusHead identification header (RFC 7845, section 5.1) as extradata. It is
 *   synthesized from the negotiated channel count.
 */
class NX_RTP_API OpusParser: public AudioStreamParser
{
public:
    // RFC 7587, section 4.1: an Opus RTP stream always uses a 48 kHz clock, whatever the encoder's
    // internal sample rate is.
    static constexpr int kClockRate = 48000;

    OpusParser() { StreamParser::setFrequency(kClockRate); }

    virtual void setSdpInfo(const Sdp::Media& sdp) override;
    virtual Result processData(const RtpHeader& rtpHeader,
        // `uint8_t` is the same type as the base class's `quint8`, spelled without Qt.
        uint8_t* rtpBufferBase,
        int bufferOffset,
        int bufferSize,
        bool& gotData) override;
    virtual void clear() override {}
    virtual CodecParametersConstPtr getCodecParameters() override { return m_context; }

private:
    CodecParametersConstPtr m_context;
    int m_channels = 1;
};

} // namespace nx::rtp
