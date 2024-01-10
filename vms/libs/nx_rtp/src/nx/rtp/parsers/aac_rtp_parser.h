// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/rtp/parsers/rtp_stream_parser.h>

namespace nx::rtp {

class NX_RTP_API AacParser: public AudioStreamParser
{
public:
    AacParser();
    virtual void setSdpInfo(const Sdp::Media& sdp) override;
    virtual Result processData(
        const RtpHeader& rtpHeader,
        quint8* rtpBufferBase,
        int bufferOffset,
        int bytesRead,
        bool& gotData) override;
    virtual void clear() override {}
    virtual CodecParametersConstPtr getCodecParameters() override;

private:
    int m_sizeLength = 0; // 0 if constant size. see RFC3640
    int m_constantSize = 0;
    int m_indexLength = 0;
    int m_indexDeltaLength = 0;
    int m_CTSDeltaLength = 0;
    int m_DTSDeltaLength = 0;
    int m_randomAccessIndication = 0;
    int m_streamStateIndication = 0;
    int m_profile = 0;
    int m_bitrate = 0;
    int m_channels = 0;
    int m_streamtype = 0;
    bool m_auHeaderExists = false;

    CodecParametersPtr m_context;
};

} // namespace nx::rtp
