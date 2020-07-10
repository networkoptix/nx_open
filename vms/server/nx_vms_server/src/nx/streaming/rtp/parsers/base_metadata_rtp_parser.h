#include <set>

#include <nx/utils/buffer.h>

#include <nx/streaming/rtp/parsers/rtp_stream_parser.h>

namespace nx::streaming::rtp {

class BaseMetadataRtpParser: public StreamParser
{
    static constexpr int kInvalidTimestamp = -1;

public:
    virtual void setSdpInfo(const Sdp::Media& sdp) override;

    virtual QnAbstractMediaDataPtr nextData() override;

    virtual Result processData(
        quint8* rtpBufferBase,
        int bufferOffset,
        int bytesRead,
        bool& gotData) override;

private:
    void cleanUpOnError();

    QnCompressedMetadataPtr makeCompressedMetadata();

private:
    Sdp::Media m_sdp;
    nx::Buffer m_buffer;
    int32_t m_currentDataChunkTimestamp = kInvalidTimestamp;
    QnCompressedMetadataPtr m_metadata;
};

} // namespace nx::streaming::rtp
