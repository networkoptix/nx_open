// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/codec/nal_units.h>
#include <nx/media/video_data_packet.h>
#include <nx/rtp/parsers/rtp_stream_parser.h>

namespace nx::rtp {

class NX_RTP_API Mpeg4Parser: public VideoStreamParser
{
public:
    Mpeg4Parser();
    virtual void setSdpInfo(const Sdp::Media& sdp) override;

    virtual Result processData(
        const RtpHeader& rtpHeader,
        quint8* rtpBufferBase,
        int bufferOffset,
        int bytesRead,
        bool& gotData) override;
    virtual void clear() override;

private:

private:
    CodecParametersConstPtr m_codecParameters;
    QnWritableCompressedVideoDataPtr m_packet;
    int m_width = 0;
    int m_height = 0;
};

} // namespace nx::rtp
