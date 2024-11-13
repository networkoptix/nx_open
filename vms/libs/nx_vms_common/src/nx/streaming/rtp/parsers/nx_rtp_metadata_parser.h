// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/rtp/parsers/rtp_stream_parser.h>

namespace nx::rtp {

class NX_VMS_COMMON_API NxRtpMetadataParser: public StreamParser
{
    using base_type = StreamParser;

public:
    /** @param tag Human-readable tag for logging. */
    NxRtpMetadataParser() = default;

    virtual void setSdpInfo(const Sdp::Media& sdp) override {}

    virtual bool isUtcTime() const override { return true; }

    virtual Result processData(
        const RtpHeader& rtpHeader,
        quint8* rtpBufferBase,
        int bufferOffset,
        int bytesRead,
        bool& gotData) override;

    virtual QnAbstractMediaDataPtr nextData() override;

    virtual void clear() override {}
};

using NxRtpMetadataParserPtr = QSharedPointer<NxRtpMetadataParser>;

} // namespace nx::rtp
