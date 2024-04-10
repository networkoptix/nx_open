// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stun_extension_types.h"

#include <nx/utils/string.h>

namespace nx::network::stun::extension {

namespace methods {

std::string_view toString(Value val)
{
    switch (val)
    {
        case reserved1:
            return "reserved1";
        case bind:
            return "bind";
        case listen:
            return "listen";
        case connectionAck:
            return "connectionAck";
        case resolvePeer:
            return "resolvePeer";
        case resolveDomain:
            return "resolveDomain";
        case connect:
            return "connect";
        case connectionResult:
            return "connectionResult";
        case udpHolePunchingSyn:
            return "udpHolePunchingSyn";
        case tunnelConnectionChosen:
            return "tunnelConnectionChosen";
        default:
            return "unknown";
    };
}

} // namespace methods

namespace attrs {

const char* toString(AttributeType val)
{
    switch (val)
    {
        case resultCode:
            return "resultCode";
        case systemId:
            return "systemId";
        case serverId:
            return "serverId";
        case peerId:
            return "peerId";
        case connectionId:
            return "connectionId";
        case cloudConnectVersion:
            return "cloudConnectVersion";

        case hostName:
            return "hostName";
        case hostNameList:
            return "hostNameList";
        case publicEndpointList:
            return "publicEndpointList";
        case tcpHpEndpointList:
            return "tcpHpEndpointList";
        case udtHpEndpointList:
            return "udtHpEndpointList";
        case connectionMethods:
            return "connectionMethods";
        case ignoreSourceAddress:
            return "ignoreSourceAddress";
        case tcpReverseEndpointList:
            return "tcpReverseEndpointList";
        case isPersistent:
            return "isPersistent";
        case isListening:
            return "isListening";
        case trafficRelayUrl:
            return "trafficRelayUrl";
        case trafficRelayUrlList:
            return "trafficRelayUrlList";
        case trafficRelayConnectTimeout:
            return "trafficRelayConnectTimeout";

        case udpHolePunchingResultCode:
            return "udpHolePunchingResultCode";
        case rendezvousConnectTimeout:
            return "rendezvousConnectTimeout";
        case udpTunnelKeepAliveInterval:
            return "udpTunnelKeepAliveInterval";
        case udpTunnelKeepAliveRetries:
            return "udpTunnelKeepAliveRetries";
        case tcpReverseRetryMaxCount:
            return "tcpReverseRetryMaxCount";
        case tcpReverseRetryInitialDelay:
            return "tcpReverseRetryInitialDelay";
        case tcpReverseRetryDelayMultiplier:
            return "tcpReverseRetryDelayMultiplier";
        case tcpReverseRetryMaxDelay:
            return "tcpReverseRetryMaxDelay";
        case tcpReverseHttpSendTimeout:
            return "tcpReverseHttpSendTimeout";
        case tcpReverseHttpReadTimeout:
            return "tcpReverseHttpReadTimeout";
        case tcpReverseHttpMsgBodyTimeout:
            return "tcpReverseHttpMsgBodyTimeout";
        case tunnelInactivityTimeout:
            return "tunnelInactivityTimeout";
        case tcpConnectionKeepAlive:
            return "tcpConnectionKeepAlive";

        case udpHolePunchingStartDelay:
            return "udpHolePunchingStartDelay";
        case trafficRelayingStartDelay:
            return "trafficRelayingStartDelay";
        case directTcpConnectStartDelay:
            return "directTcpConnectStartDelay";
        case connectType:
            return "connectType";
        case httpStatusCode:
            return "httpStatusCode";
        case duration:
            return "duration";
        case systemErrorCode:
            return "systemErrorCode";

        default:
            return "unknown";
    }

} // namespace attrs

BaseStringAttribute::BaseStringAttribute(int userType, const std::string& value):
    stun::attrs::Unknown(userType, nx::Buffer(value))
{
}

//-------------------------------------------------------------------------------------------------

Endpoint::Endpoint(int type, const SocketAddress& endpoint):
    BaseStringAttribute(type, endpoint.toString())
{
}

SocketAddress Endpoint::get() const
{
    return SocketAddress(getString());
}

//-------------------------------------------------------------------------------------------------

EndpointList::EndpointList(int type, const std::vector<SocketAddress>& endpoints):
    BaseStringAttribute(
        type,
        nx::utils::join(endpoints.begin(), endpoints.end(), ','))
{
}

std::vector<SocketAddress> EndpointList::get() const
{
    const auto value = getString();
    if (value.empty())
        return {};

    std::vector<SocketAddress> endpoints;
    nx::utils::split(
        value, ',',
        [&endpoints](const std::string_view& token)
        {
            endpoints.push_back(SocketAddress(token));
        });

    return endpoints;
}

StringList::StringList(int type, const std::vector<std::string>& strings):
    BaseStringAttribute(type, nx::utils::join(strings.begin(), strings.end(), ','))
{
}

std::vector<std::string> StringList::get() const
{
    const auto value = getString();
    std::vector<std::string> tokens;
    nx::utils::split(
        value, ',',
        [&tokens](const std::string_view& token) { tokens.push_back(std::string(token)); });
    return tokens;
}

} // namespace attrs
} // namespace nx::network::stun::extension
