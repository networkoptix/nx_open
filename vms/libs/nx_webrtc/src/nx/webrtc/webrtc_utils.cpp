// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_utils.h"

#include <set>

namespace {

/* Split string into tokens separated by 'delim' argument,
 * with whitespaces trimming, and without copying.
 * Example:
 * "abc \r\n def\r\nghi \r\n jkl \r\nmno\r\n\r\n" ->
 * "abc", "def", "ghi", "jkl", "mno"
 * */
template <typename Callback>
void getTokens(const std::string_view& str, char delim, Callback callback)
{
    if (str.begin() == str.end())
        return;

    const char* start = &str[0];
    const char* stop = start;
    const char* end = start + str.size();
    while (start != end)
    {
        while (start != end && (*start <= 0x20 || *start == delim)) ++start;
        stop = start;
        while (stop != end && *stop != delim) ++stop;
        while (stop != start && *(stop - 1) <= 0x20) --stop;
        callback(std::string_view(start, stop - start));
        start = stop;
        while (start != end && (*start <= 0x20 || *start == delim)) ++start;
    }
}

} // namespace

namespace nx::webrtc {

Fingerprint Fingerprint::parse(const std::string_view& sv)
{
    // a=fingerprint:sha-1 4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB
    static const std::string_view kFingerprint("a=fingerprint");
    struct HashInfo
    {
        std::string_view name = "";
        Type type = Type::none;
        int size = 0;
    };
    static constexpr std::array<HashInfo, 7> hashInfos =
    {{
        {"sha-1", Type::sha1, 20},
        {"sha-224", Type::sha224, 28},
        {"sha-256", Type::sha256, 32},
        {"sha-384", Type::sha384, 48},
        {"sha-512", Type::sha512, 64},
        {"md5", Type::md5, 16},
        {"md2", Type::md2, 16}
    }};
    Fingerprint result;
    int i = 0;
    bool error = false;
    auto hashInfo = hashInfos.end();
    getTokens(
        sv,
        ' ',
        [&](const std::string_view& bySpace)
        {
            getTokens(
                bySpace,
                ':',
                [&](const std::string_view& mid)
                {
                    if (error)
                        return;
                    if (i == 0)
                    {
                        if (mid != kFingerprint)
                            error = true;
                        else
                            ++i;
                        return;
                    }
                    if (i == 1)
                    {
                        hashInfo = std::find_if(
                            hashInfos.begin(),
                            hashInfos.end(),
                            [&mid](const auto& currentHashInfo)
                            {
                                return currentHashInfo.name == mid;
                            });
                        if (hashInfo == hashInfos.end())
                        {
                            error = true;
                            return;
                        }
                        result.type = hashInfo->type;
                        result.hash.reserve(hashInfo->size);
                    }
                    if (i - 2 >= hashInfo->size)
                    {
                        error = true;
                        return;
                    }
                    if (i > 1)
                    {
                        if (mid.size() != 2)
                        {
                            error = true;
                            return;
                        }
                        int octet = 0;
                        for (int j = 0; j < 2; ++j)
                        {
                            int half = 0;
                            if (mid[j] >= '0' && mid[j] <= '9')
                                half = mid[j] - '0';
                            else if (mid[j] >= 'A' && mid[j] <= 'F')
                                half = mid[j] - 'A' + 10;
                            else
                                error = true;
                            octet <<= 4;
                            octet += half;
                        }
                        result.hash.push_back((uint8_t) octet);
                    }
                    ++i;
                });
        });
    if (!error && result.hash.size() != (size_t) hashInfo->size)
        error = true;
    if (error)
        return {};
    return result;
}

// Actually extracting only 'ice-ufrag' and 'ice-pwd' fields, and checks for 'a=inactive'
SdpParseResult SdpParseResult::parse(const std::string& sdp)
{
    /**
     * The check for active/inactive/absent track is tricky because of
     * there are some differences between various browser's SDP answers.
     * Chrome always add 'a=group' attribute, but exclude from group mids (Media IDs) of tracks
     * which will be inactive (i.e. track uses unsupported codec).
     * Also, old Chrome and Safari raises an JavaScript exception on this event,
     * which is handled not in this function.
     * And finally. Firefox marks inactive tracks with attribute, and don't add
     * 'group' attribute even if there is one inactive track.
     */
    static const std::string_view kIcePwdPrefix("a=ice-pwd:");
    static const std::string_view kIceUfragPrefix("a=ice-ufrag:");
    static const std::string_view kAttributeInactive("a=inactive");
    static const std::string_view kVideo("m=video");
    static const std::string_view kAudio("m=audio");
    static const std::string_view kApplication("m=application");
    static const std::string_view kGroup("a=group");
    static const std::string_view kMid("a=mid:");
    static const std::string_view kFingerprint("a=fingerprint:");

    SdpParseResult result;
    SdpParseResult::TrackState* currentTrackState = nullptr;
    bool hasGroup = false;
    std::set<std::string> groupedMids;

    getTokens(
        sdp,
        '\n',
        [&](const std::string_view& sv)
        {
            if (sv.find(kIcePwdPrefix) == 0)
            {
                result.icePwd = sv.substr(kIcePwdPrefix.size());
            }
            else if (sv.find(kIceUfragPrefix) == 0)
            {
                result.iceUfrag = sv.substr(kIceUfragPrefix.size());
            }
            else if (sv.find(kAttributeInactive) == 0)
            {
                if (currentTrackState != nullptr)
                    *currentTrackState = SdpParseResult::TrackState::inactive;
            }
            else if (sv.find(kVideo) == 0)
            {
                currentTrackState = &result.video;
                *currentTrackState = SdpParseResult::TrackState::active;
            }
            else if (sv.find(kAudio) == 0)
            {
                currentTrackState = &result.audio;
                *currentTrackState = SdpParseResult::TrackState::active;
            }
            else if (sv.find(kApplication) == 0)
            {
                currentTrackState = &result.application;
                *currentTrackState = SdpParseResult::TrackState::active;
            }
            else if (sv.find(kGroup) == 0)
            {
                hasGroup = true;
                int i = 0;
                getTokens(
                    sv,
                    ' ',
                    [&](const std::string_view& mid)
                    {
                        if (i++)
                            groupedMids.emplace(mid);
                    });
            }
            else if (sv.find(kMid) == 0)
            {
                if (hasGroup
                    && groupedMids.count(std::string(sv, kMid.size(), std::string::npos)) == 0
                    && currentTrackState != nullptr)
                {
                    // Assume that non-grouped mid's are inactive.
                    *currentTrackState = SdpParseResult::inactive;
                }
            }
            else if (sv.find(kFingerprint) == 0)
            {
                result.fingerprint = Fingerprint::parse(sv);
            }
        });

    return result;
}

IceCandidate IceCandidate::constructTcpHost(
        unsigned int _foundation,
        int _priority,
        const nx::network::SocketAddress& _address)
{
    return IceCandidate{
        .foundation = _foundation,
        .componentId = 1,
        .protocol = Protocol::TCP,
        .priority = _priority,
        .address = _address,
        .type = Type::Host,
        .tcpType = TcpType::Passive};
}

IceCandidate IceCandidate::constructUdpHost(
        unsigned int _foundation,
        int _priority,
        const nx::network::SocketAddress& _address)
{
    return IceCandidate{
        .foundation = _foundation,
        .componentId = 1,
        .protocol = Protocol::UDP,
        .priority = _priority,
        .address = _address,
        .type = Type::Host};

}

IceCandidate IceCandidate::constructUdpSrflx(
        unsigned int _foundation,
        int _priority,
        const nx::network::SocketAddress& _address,
        const nx::network::SocketAddress& _relatedAddress)
{
    return IceCandidate{
        .foundation = _foundation,
        .componentId = 1,
        .protocol = Protocol::UDP,
        .priority = _priority,
        .address = _address,
        .type = Type::Srflx,
        .relatedAddress = _relatedAddress};
}

// Examples:
// candidate:0 1 TCP 2147483647 192.168.0.6 7001 typ host tcptype passive
// candidate:4 1 TCP 2147483643 0000::0000:000:0000:0000 7001 typ host tcptype passive
// candidate:5 1 UDP 2147483642 192.168.0.6 46027 typ host
// candidate:6 1 UDP 2147483641 42.42.42.42 16909 typ srflx raddr 0.0.0.0 rport 50941
IceCandidate IceCandidate::parse(const std::string& candidate)
{
    IceCandidate result;
    enum State
    {
        Foundation,
        ComponentId,
        Protocol,
        Priority,
        Address,
        Port,
        Type,
        Tcptype,
        RAddress,
        RPort,
        Unknown
    };
    State state{State::Foundation};
    std::string_view address;
    getTokens(
        candidate,
        ' ',
        [&](const std::string_view& sv)
        {
            switch (state)
            {
                case State::Foundation:
                {
                    int i = 0;
                    getTokens(
                        sv,
                        ':',
                        [&](const std::string_view& sp)
                        {
                            if (i++)
                                result.foundation = std::strtol(sp.data(), nullptr, 10);
                        });
                    state = State::ComponentId;
                    break;
                }
                case State::ComponentId:
                {
                    result.componentId = std::strtol(sv.data(), nullptr, 10);
                    state = State::Protocol;
                    break;
                }
                case State::Protocol:
                {
                    if (sv == "TCP" || sv == "tcp")
                        result.protocol = Protocol::TCP;
                    // ...Else assume its UDP.
                    state = State::Priority;
                    break;
                }
                case State::Priority:
                {
                    result.priority = std::strtol(sv.data(), nullptr, 10);
                    state = State::Address;
                    break;
                }
                case State::Address:
                {
                    address = sv;
                    state = State::Port;
                    break;
                }
                case State::Port:
                {
                    result.address = nx::network::SocketAddress(
                        nx::network::HostAddress(address),
                        std::strtol(sv.data(), nullptr, 10));
                    state = State::Type;
                    break;
                }
                case State::Type:
                {
                    if (sv == "typ")
                        break;
                    if (sv == "srflx")
                        result.type = Type::Srflx;
                    else if (sv == "relay")
                        result.type = Type::Relay;
                    // ...Else assume its Host.
                    state = State::Unknown;
                    break;
                }
                case State::Tcptype:
                {
                    if (sv == "active")
                        result.tcpType = TcpType::Active;
                    else if (sv == "passive")
                        result.tcpType = TcpType::Passive;
                    else if (sv == "actpass")
                        result.tcpType = TcpType::Actpass;
                    state = State::Unknown;
                    break;
                }
                case State::RAddress:
                {
                    address = sv;
                    state = State::RPort;
                    break;
                }
                case State::RPort:
                {
                    if (sv == "rport")
                        break;
                    result.relatedAddress = nx::network::SocketAddress(
                        nx::network::HostAddress(address),
                        std::strtol(sv.data(), nullptr, 10));
                    state = State::Unknown;
                    break;
                }
                case State::Unknown:
                {
                    if (sv == "tcptype")
                        state = State::Tcptype;
                    else if (sv == "raddr")
                        state = State::RAddress;
                    break;
                }
            }
        });
    return result;
}

std::string IceCandidate::serialize() const
{
    std::string candidate = "candidate:";
    candidate += std::to_string(foundation);
    candidate += " ";
    candidate += std::to_string(componentId);
    candidate += (protocol == Protocol::UDP ? " UDP " : " TCP ");
    candidate += std::to_string(priority);
    candidate += " ";
    candidate += address.address.toString();
    candidate += " ";
    candidate += std::to_string(address.port);
    switch (type)
    {
        case Type::Host:
            candidate += " typ host";
            break;
        case Type::Srflx:
        case Type::Prflx:
            candidate += " typ srflx";
            break;
        case Type::Relay:
            candidate += " typ relay";
            break;
    }
    if (relatedAddress)
    {
        candidate += " raddr ";
        candidate += relatedAddress->address.toString();
        candidate += " rport ";
        candidate += std::to_string(relatedAddress->port);
    }
    if (tcpType)
    {
        switch (*tcpType)
        {
            case TcpType::Active:
                candidate += " tcptype active";
                break;
            case TcpType::Passive:
                candidate += " tcptype passive";
                break;
            case TcpType::Actpass:
                candidate += " tcptype actpass";
                break;
        }
    }
    return candidate;
}

bool IceCandidate::matchFilter(const Filter filter) const
{
    Filter actualType = Filter::None;
    if (type == Type::Host)
    {
        if (protocol == Protocol::TCP)
           actualType = Filter::TcpHost;
        else
           actualType = Filter::UdpHost;
    }
    else if (type == Type::Srflx)
    {
        actualType = Filter::UdpSrflx;
    }
    else if (type == Type::Relay)
    {
        actualType = Filter::UdpRelay;
    }
    return (filter & actualType) != Filter::None;
}

IceCandidate::Filter IceCandidate::parseFilter(const std::string& filter)
{
    int result = Filter::None;
    static const std::string_view kTcp = "tcp";
    static const std::string_view kUdp = "udp";
    static const std::string_view kSrflx = "srflx";
    static const std::string_view kRelay = "relay";

    getTokens(
        filter,
        ',',
        [&](const std::string_view& sv)
        {
            if (sv == kUdp)
                result |= Filter::UdpHost;
            else if (sv == kTcp)
                result |= Filter::TcpHost;
            else if (sv == kSrflx)
                result |= Filter::UdpSrflx;
            else if (sv == kRelay)
                result |= Filter::UdpRelay;
        });
    return (IceCandidate::Filter)result;
}

} // namespace nx::webrtc
