#include <set>
#include <optional>

#include <nx/utils/buffer.h>

#include <nx/streaming/rtp/parsers/rtp_stream_parser.h>

namespace nx::streaming::rtp {

class BaseMetadataRtpParser: public StreamParser
{

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

    QnCompressedMetadataPtr makeCompressedMetadata(int32_t timestamp);

private:
    Sdp::Media m_sdp;
    nx::Buffer m_buffer;
    std::optional<uint32_t> m_previousDataChunkTimestamp;
    bool m_trustMarkerBit = false;
    QnCompressedMetadataPtr m_metadata;
};

} // namespace nx::streaming::rtp
