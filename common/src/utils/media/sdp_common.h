#pragma once

#include <nx/network/socket_common.h>

namespace nx {
namespace network {
namespace sdp {

enum class NetworkType
{
    Undefined,
    Other,
    Internet
};

enum class AddressType
{
    Undefined,
    Other,
    IpV4,
    IpV6
};

enum class BandwidthType
{
    Undefined,
    Other,
    Experimental,
    ConferenceTotal,
    ApplicationSpecific
};

enum class MediaType
{
    Undefined,
    Other,
    Audio,
    Video,
    Text,
    Application,
    Message,
};

enum class Proto
{
    Undefined,
    Other,
    Udp,
    RtpBasedProtoStart, //< service value, no real proto present
    RtpAvp,
    RtpSavp
};

struct ConnectionInfo
{
    NetworkType networkType = NetworkType::Undefined;
    AddressType addressType = AddressType::Undefined;
    HostAddress address;
    int ttl = 0;
    int numberOfAddresses = 1;

    static NetworkType fromStringToNetworkType(const QString& str);
    static AddressType fromStringToAddressType(const QString& str);
};

bool operator==(const ConnectionInfo& first, const ConnectionInfo& second);

struct Bandwidth
{
    BandwidthType bandwidthType = BandwidthType::Undefined;
    int bandwidthKbitPerSecond = 0;

    static BandwidthType fromStringToBandwidthType(const QString& str);
};

bool operator==(const Bandwidth& first, const Bandwidth& second);

struct Timing
{
    int64_t startTimeSecondsSinceEpoch = 0;
    int64_t endTimeSecondsSinceEpoch = 0;
};

bool operator==(const Timing& first, const Timing& second);

struct RepeatTimes
{
    int64_t repeatIntervalSeconds = 0;
    int64_t activeDurationSeconds = 0;
    std::vector<int64_t> offsetsFromStartTimeSeconds;
};

bool operator==(const RepeatTimes& first, const RepeatTimes& second);

struct TimeZone
{
    int64_t adjustmentTimeSeconds = 0;
    int64_t offsetSeconds = 0;
};

bool operator==(const TimeZone& first, const TimeZone& second);

struct MediaParams
{
    MediaType mediaType = MediaType::Undefined;
    int port = 0;
    int numberOfPorts = 1;
    Proto proto = Proto::Undefined;
    QString format;
    std::vector<int> rtpPayloadTypes;

    static MediaType fromStringToMediaType(const QString& str);
    static Proto fromStringToProto(const QString& str);
    static bool isRtpBasedProto(Proto proto);
};

bool operator==(const MediaParams& first, const MediaParams& second);


} // namespace sdp
} // namespace network
} // namespace nx
