// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/rtp/time_linearizer.h>
#include <nx/rtp/parsers/rtp_stream_parser.h>
#include <nx/rtp/result.h>
#include <nx/rtp/rtcp.h>
#include <nx/rtp/rtp.h>

namespace nx::rtp {

class NX_RTP_API RtpParser
{
public:
    RtpParser(int payloadType, StreamParserPtr codecParser);
    Result processData(uint8_t* buffer, int offset, int size, bool& packetLoss, bool& gotData);
    void clear();
    QnAbstractMediaDataPtr nextData(const nx::rtp::RtcpSenderReport& senderReport);
    QString idForToStringFromPtr() const;
    bool isUtcTime();

private:
    Result processRtpExtension(
        const RtpHeaderExtensionHeader& extensionHeader, quint8* data, int size);
    int64_t getTimestamp(uint32_t rtpTime, const nx::rtp::RtcpSenderReport& senderReport);

private:
    StreamParserPtr m_codecParser;
    nx::rtp::TimeLinearizer<uint32_t> m_linearizer{0};
    std::optional<uint16_t> m_sequenceNumber;
    int m_payloadType = 0;
    bool m_isUtcTime = false;
    std::optional<std::chrono::microseconds> m_onvifExtensionTimestamp;
};

} // namespace nx::rtp
