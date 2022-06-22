// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <utility>
#include <optional>

#include <QtCore/QtGlobal>
#include <QtCore/QtEndian>

#ifdef Q_OS_WIN
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#endif

namespace nx::streaming::rtp {

// 1900 - 1970 time diff
static const std::chrono::seconds kNtpEpochTimeDiff(2208988800);

using rtptime = std::chrono::duration<int32_t, std::ratio<1, 90000>>;

#pragma pack(push, 1)
struct RtpHeader
{
    static const int kSize = 12;
    static const int kVersion = 2;
    static const int kCsrcSize = 4;
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
    unsigned short   version:2;     /* packet type                */
    unsigned short   padding:1;     /* padding flag               */
    unsigned short   extension:1;   /* header extension flag      */
    unsigned short   CSRCCount:4;   /* CSRC count                 */
    unsigned short   marker:1;      /* marker bit                 */
    unsigned short   payloadType:7; /* payload type               */
#else
    unsigned short   CSRCCount:4;   /* CSRC count                 */
    unsigned short   extension:1;   /* header extension flag      */
    unsigned short   padding:1;     /* padding flag               */
    unsigned short   version:2;     /* packet type                */
    unsigned short   payloadType:7; /* payload type               */
    unsigned short   marker:1;      /* marker bit                 */
#endif
    uint16_t sequence;               // sequence number
    uint32_t timestamp;              // timestamp
    uint32_t ssrc;                   // synchronization source
    //uint32_t csrc;                  // synchronization source
    //uint32_t csrc[1];               // optional CSRC list

    uint32_t payloadOffset() const { return kSize + CSRCCount * kCsrcSize; }
    uint32_t getTimestamp() const { return ntohl(timestamp); }
    uint16_t getSequence() const { return ntohs(sequence); }
    bool isRtcp() const
    {
        // TODO #lbusygin: Incorrect check.
        return payloadType >= 72 && payloadType <= 76;
    }

    static std::optional<int> getPayloadType(const uint8_t* data, int size)
    {
        if (!data || size < kSize)
            return {};
        const RtpHeader* header = (RtpHeader*)(data);
        std::optional<int> res;
        res.emplace(header->payloadType);
        return res;
    }
};

struct RtpHeaderExtensionHeader
{
    static const int kSize = 4;
    uint16_t definedByProfile; //< Name from RFC. Actually it is extension type id.
    uint16_t length; //< Length in 4-byte words.

    int id() const { return ntohs(definedByProfile); }
    int lengthBytes() const { return ntohs(length) * /*size of word*/ 4; }
};
#pragma pack(pop)

struct RtpExtensionData
{
    RtpHeaderExtensionHeader header;
    int extensionOffset = 0; //< Offset of extension data, exclude extension header
    int extensionSize = 0; //< Size of extension data, exclude extension header
};

struct NX_VMS_COMMON_API RtpHeaderData
{
    bool read(const uint8_t* data, int size);
    RtpHeader header;
    int payloadOffset = 0;
    int payloadSize = 0;
    std::optional<RtpExtensionData> extension;
};

inline std::pair<uint32_t, uint32_t> makeFraction(uint64_t value, uint64_t denominator)
{
    std::pair<uint32_t, uint32_t> result;
    result.first = value / denominator;
    result.second = ((value % denominator) << 32) / denominator;
    return result;
}

inline void buildRtpHeader(
    char* buffer,
    uint32_t ssrc,
    uint8_t markerBit,
    uint32_t timestamp,
    uint8_t payloadType,
    uint16_t sequence)
{
    RtpHeader* rtp = (RtpHeader*)buffer;
    rtp->version = RtpHeader::kVersion;
    rtp->padding = 0;
    rtp->extension = 0;
    rtp->CSRCCount = 0;
    rtp->marker = markerBit;
    rtp->payloadType = payloadType;
    rtp->sequence = qToBigEndian(sequence);
    rtp->timestamp = qToBigEndian(timestamp);
    rtp->ssrc = qToBigEndian(ssrc);
}

} // namespace nx::streaming::rtp
