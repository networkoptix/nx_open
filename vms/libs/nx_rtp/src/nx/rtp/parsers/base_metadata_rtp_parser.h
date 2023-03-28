// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <set>

#include <nx/media/meta_data_packet.h>
#include <nx/rtp/parsers/rtp_stream_parser.h>
#include <nx/utils/buffer.h>

namespace nx::rtp {

class NX_RTP_API BaseMetadataRtpParser: public StreamParser
{

public:
    virtual void setSdpInfo(const Sdp::Media& sdp) override;

    virtual QnAbstractMediaDataPtr nextData() override;

    virtual Result processData(
        const RtpHeader& rtpHeader,
        quint8* rtpBufferBase,
        int bufferOffset,
        int bytesRead,
        bool& gotData) override;

    virtual void clear() override;

private:
    QnCompressedMetadataPtr makeCompressedMetadata(int32_t timestamp);

private:
    Sdp::Media m_sdp;
    QByteArray m_buffer;
    std::optional<uint32_t> m_previousDataChunkTimestamp;
    bool m_trustMarkerBit = false;
    QnCompressedMetadataPtr m_metadata;
};

} // namespace nx::rtp
