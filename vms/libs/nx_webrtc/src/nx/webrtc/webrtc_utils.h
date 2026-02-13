// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <string>

#include <nx/network/socket_common.h>
#include <nx/network/ssl/certificate.h>

namespace nx::webrtc {

constexpr uint16_t kDefaultSctpPort = 5000;
constexpr int kSctpMaxMessageSize = 65536;

using namespace nx::network;
using namespace nx;

enum class Purpose
{
    send,
    recv,
    sendrecv,
};

class Dtls;

class Session;
class SessionPool;

using SessionPtr = std::shared_ptr<Session>;

struct SessionDescription
{
    std::string id;
    std::string remoteUfrag;
    std::string localPwd;
    std::shared_ptr<Dtls> dtls;
};

struct Fingerprint
{
    std::vector<uint8_t> hash; //< In binary format.
    NX_REFLECTION_ENUM_IN_CLASS(Type,
        sha1,
        sha224,
        sha256,
        sha384,
        sha512,
        md5,
        md2,
        none)
    Type type = Type::none;
    static Fingerprint parse(const std::string_view& sv);
};

enum TrackState
{
    offer, //< Offer sent but state is not confirmed yet
    inactive,
    active,
};

struct SdpParseResult
{
    Fingerprint fingerprint;
    std::string iceUfrag;
    std::string icePwd;
    std::map<int, TrackState> tracksState;

    static SdpParseResult parse(const std::string& sdp);
};

struct IceCandidate
{
    static constexpr int kIceMaxPriority = std::numeric_limits<int>::max();
    static constexpr int kIceMinIndex = 0;

    // Debug purpose.
    enum Filter: int
    {
        None = 0,
        UdpHost = 1,
        TcpHost = 2,
        UdpSrflx = 4,
        UdpRelay = 8,
        All = UdpHost | TcpHost | UdpSrflx | UdpRelay,
    };
    enum Protocol
    {
        UDP,
        TCP
    };
    enum Type
    {
        Host,
        Srflx,
        Prflx, //< Not sure if this type can exist in SDP.
        Relay
    };
    enum TcpType
    {
        Active,
        Passive,
        Actpass
    };

    unsigned int foundation = 0; //< Something like an ID.
    int componentId = 1; //< 1 for RTP, 2 for RTCP. Always 1 for rtcp-mux
    Protocol protocol = Protocol::UDP;
    int priority = 0;
    nx::network::SocketAddress address;
    Type type = Type::Host;
    std::optional<TcpType> tcpType;
    std::optional<nx::network::SocketAddress> relatedAddress;

    static IceCandidate constructTcpHost(
        unsigned int _foundation,
        int _priority,
        const nx::network::SocketAddress& _address);
    static IceCandidate constructUdpHost(
        unsigned int _foundation,
        int _priority,
        const nx::network::SocketAddress& _address);
    static IceCandidate constructUdpSrflx(
        unsigned int _foundation,
        int _priority,
        const nx::network::SocketAddress& _address,
        const nx::network::SocketAddress& _relatedAddress);

    static IceCandidate parse(const std::string& candidate);
    static Filter parseFilter(const std::string& filter);
    std::string serialize() const;
    bool matchFilter(const Filter filter) const;
};

struct TurnServerInfo
{
    nx::network::SocketAddress address;
    std::string user;
    std::string password;
    std::string realm;
    bool longTermAuth = false;
};

struct OauthInfo
{
    std::string kid;
    int lifetime;
    std::chrono::seconds expires_at;
    std::string sessionKey;
    std::string accessToken;
};

struct SessionConfig
{
    bool unreliableTransport = false;
};

} // namespace nx::webrtc
