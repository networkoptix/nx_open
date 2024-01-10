// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/rtp/parsers/rtp_stream_parser.h>

namespace nx::rtp {

// used for G711, G726 e.t.c simple audio codecs with one frame per packet
class NX_RTP_API SimpleAudioParser: public AudioStreamParser
{
public:
    SimpleAudioParser(AVCodecID codecId);
    virtual void setSdpInfo(const Sdp::Media& sdp) override;
    virtual Result processData(
        const RtpHeader& rtpHeader,
        quint8* rtpBufferBase,
        int bufferOffset,
        int bytesRead,
        bool& gotData) override;
    virtual void clear() override {}
    virtual CodecParametersConstPtr getCodecParameters() override;
    void setBitsPerSample(int value);

private:
    CodecParametersConstPtr m_context;
    int m_channels = 1;
    int m_bits_per_coded_sample = 0;
    int m_bitrate = 0;
    AVCodecID m_codecId = AV_CODEC_ID_PCM_MULAW;
};

} // namespace nx::rtp
