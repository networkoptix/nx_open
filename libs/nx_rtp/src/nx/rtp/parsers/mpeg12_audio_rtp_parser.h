// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/rtp/parsers/mpeg12_audio_header_parser.h>
#include <nx/rtp/parsers/rtp_stream_parser.h>

namespace nx::rtp {

class NX_RTP_API Mpeg12AudioParser: public AudioStreamParser
{
    struct DataChunk
    {
        const uint8_t* start = nullptr;
        int length = 0;
    };

public:
    Mpeg12AudioParser();
    virtual void setSdpInfo(const Sdp::Media& sdp) override;
    virtual Result processData(
        const RtpHeader& rtpHeader,
        quint8* rtpBufferBase,
        int bufferOffset,
        int bytesRead,
        bool& gotData) override;
    virtual void clear() override;
    virtual CodecParametersConstPtr getCodecParameters() override;

private:
    void createAudioFrame();
    Result updateFromMpegAudioHeader(const Mpeg12AudioHeader& mpegAudioHeader);

private:
    CodecParametersConstPtr m_mediaContext;
    quint32 m_rtpTimestamp = 0;
    int m_audioFrameSize = 0;
    bool m_skipFragmentsUntilNextAudioFrame = true;

    std::vector<DataChunk> m_audioDataChunks;
};

} // namespace nx::rtp
