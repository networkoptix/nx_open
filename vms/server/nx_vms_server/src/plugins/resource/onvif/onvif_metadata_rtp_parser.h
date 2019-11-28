#pragma once

#include <set>

#include <nx/utils/buffer.h>

#include <nx/streaming/rtp/parsers/rtp_stream_parser.h>

class OnvifMetadataRtpParser: public nx::streaming::rtp::StreamParser
{
    static constexpr int kInvalidTimestamp = -1;
public:
    static const std::set<QString> kSupportedCodecs;

public:
    virtual void setSdpInfo(const nx::streaming::Sdp::Media& sdp) override;

    virtual QnAbstractMediaDataPtr nextData() override;

    virtual bool processData(
        quint8* rtpBufferBase,
        int bufferOffset,
        int bytesRead,
        bool& gotData) override;

private:
    bool cleanUpOnError();

    QnCompressedMetadataPtr makeCompressedMetadata();

private:
    nx::streaming::Sdp::Media m_sdp;
    nx::Buffer m_buffer;
    int32_t m_currentDataChunkTimestamp = kInvalidTimestamp;
    QnCompressedMetadataPtr m_metadata;
};
