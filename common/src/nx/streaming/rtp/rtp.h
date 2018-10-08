#pragma once

#include <chrono>

namespace nx {
namespace streaming {
namespace rtp {

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
    //quint32 csrc;                 // synchronization source
    //quint32 csrc[1];              // optional CSRC list

    uint32_t payloadOffset() { return kSize + CSRCCount * 4; };
};
#pragma pack(pop)

#pragma pack(push, 1)
struct RtpHeaderExtension
{
    static const int kSize = 4;
    uint16_t definedByProfile; //< Name from RFC. Actually it is extension type id.
    uint16_t length; //< Length in 4-byte words.
};
#pragma pack(pop)


inline std::pair<uint32_t, uint32_t> makeFraction(uint64_t value, uint64_t denominator)
{
    std::pair<uint32_t, uint32_t> result;
    result.first = value / denominator;
    result.second = ((value % denominator) << 32) / denominator;
    return result;
}

inline void buildRtpHeader(
    char* buffer,
    quint32 ssrc,
    int markerBit,
    quint32 timestamp,
    quint8 payloadType,
    quint16 sequence)
{
    RtpHeader* rtp = (RtpHeader*)buffer;
    rtp->version = RtpHeader::kVersion;
    rtp->padding = 0;
    rtp->extension = 0;
    rtp->CSRCCount = 0;
    rtp->marker  =  markerBit;
    rtp->payloadType = payloadType;
    rtp->sequence = htons(sequence);
    rtp->timestamp = htonl(timestamp);
    rtp->ssrc = htonl(ssrc);
}

} // namespace rtp
} // namespace streaming
} // namespace nx
