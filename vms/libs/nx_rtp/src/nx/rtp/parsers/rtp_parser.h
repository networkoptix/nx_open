// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/rtp/parsers/rtp_stream_parser.h>
#include <nx/rtp/result.h>
#include <nx/rtp/rtp.h>

namespace nx::rtp {

class NX_RTP_API RtpParser
{
public:
    RtpParser(int payloadType, StreamParserPtr codecParser);
    Result processData(uint8_t* buffer, int offset, int size, bool& packetLoss, bool& gotData);
    void clear();
    int getFrequency() const;
    QnAbstractMediaDataPtr nextData();
    QString idForToStringFromPtr() const;

private:
    StreamParserPtr m_codecParser;
    std::optional<uint16_t> m_sequenceNumber;
    int m_payloadType = 0;
};

} // namespace nx::rtp
