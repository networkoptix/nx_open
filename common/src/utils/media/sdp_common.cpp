#include "sdp_common.h"

namespace nx {
namespace network {
namespace sdp {

namespace {

const static QString kInternetNetworkType = lit("IN");
const static QString kIp4AddressType = lit("IP4");
const static QString kIp6AddressType = lit("IP6");

const static QString kConferenceTotalBandwidthType = lit("CT");
const static QString kApplicationSpecificBandwidthType = lit("AS");
const static QString kExperimentalBandwidthTypePrefix = lit("X-");

const static QString kAudioMediaType = lit("audio");
const static QString kVideoMediaType = lit("video");
const static QString kTextMediaType = lit("text");
const static QString kApplicationMediaType = lit("application");
const static QString kMessageMediaType = lit("message");

const static QString kUdpProto = lit("udp");
const static QString kRtpAvpProto = lit("RTP/AVP");
const static QString kRtpSavpProto = lit("RTP/SAVP");

} // namespace

NetworkType ConnectionInfo::fromStringToNetworkType(const QString& str)
{
    if (str == kInternetNetworkType)
        return NetworkType::Internet;

    return NetworkType::Other;
}

AddressType ConnectionInfo::fromStringToAddressType(const QString& str)
{
    if (str == kIp4AddressType)
        return AddressType::IpV4;
    else if (str == kIp6AddressType)
        return AddressType::IpV6;

    return AddressType::Other;
}

MediaType MediaParams::fromStringToMediaType(const QString& str)
{
    if (str == kAudioMediaType)
        return MediaType::Audio;
    else if (str == kVideoMediaType)
        return MediaType::Video;
    else if (str == kTextMediaType)
        return MediaType::Text;
    else if (str == kApplicationMediaType)
        return MediaType::Application;
    else if (str == kMessageMediaType)
        return MediaType::Message;

    return MediaType::Other;
}

Proto MediaParams::fromStringToProto(const QString& str)
{
    if (str == kUdpProto)
        return Proto::Udp;
    else if (str == kRtpAvpProto)
        return Proto::RtpAvp;
    else if (str == kRtpSavpProto)
        return Proto::RtpSavp;

    return Proto::Other;
}

bool MediaParams::isRtpBasedProto(Proto proto)
{
    if (proto > Proto::RtpBasedProtoStart)
        return true;

    return false;
}

BandwidthType Bandwidth::fromStringToBandwidthType(const QString& str)
{
    if (str == kConferenceTotalBandwidthType)
        return BandwidthType::ConferenceTotal;
    else if (str == kApplicationSpecificBandwidthType)
        return BandwidthType::ApplicationSpecific;
    else if (str.startsWith(kExperimentalBandwidthTypePrefix))
        return BandwidthType::Experimental;

    return BandwidthType::Other;
}

bool operator==(const ConnectionInfo& first, const ConnectionInfo& second)
{
    return first.networkType == second.networkType
        && first.addressType == second.addressType
        && first.address == second.address
        && first.ttl == second.ttl
        && first.numberOfAddresses == second.numberOfAddresses;
}

bool operator==(const Bandwidth& first, const Bandwidth& second)
{
    return first.bandwidthType == second.bandwidthType
        && first.bandwidthKbitPerSecond == second.bandwidthKbitPerSecond;
}

bool operator==(const Timing& first, const Timing& second)
{
    return first.startTimeSecondsSinceEpoch == second.startTimeSecondsSinceEpoch
        && first.endTimeSecondsSinceEpoch == second.endTimeSecondsSinceEpoch;
}

bool operator==(const RepeatTimes& first, const RepeatTimes& second)
{
    return first.activeDurationSeconds == second.activeDurationSeconds
        && first.repeatIntervalSeconds == second.repeatIntervalSeconds
        && first.offsetsFromStartTimeSeconds == second.offsetsFromStartTimeSeconds;
}

bool operator==(const TimeZone& first, const TimeZone& second)
{
    return first.adjustmentTimeSeconds == second.adjustmentTimeSeconds
        && first.offsetSeconds == second.offsetSeconds;
}

bool operator==(const MediaParams& first, const MediaParams& second)
{
    return first.mediaType == second.mediaType
        && first.port == second.port
        && first.numberOfPorts == second.numberOfPorts
        && first.proto == second.proto
        && first.format == second.format;
}


} // namespace sdp
} // namespace network
} // namespace nx