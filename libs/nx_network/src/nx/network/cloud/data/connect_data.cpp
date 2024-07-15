// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connect_data.h"

#include <nx/network/stun/extension/stun_extension_types.h>

namespace nx::hpm::api {

namespace test {

ConnectRequestVersionOverride::ConnectRequestVersionOverride(CloudConnectVersion version):
    m_original(kCurrentCloudConnectVersionOverride)
{
    kCurrentCloudConnectVersionOverride = version;
}

ConnectRequestVersionOverride::~ConnectRequestVersionOverride()
{
    kCurrentCloudConnectVersionOverride = m_original;
}

} // namespace test

namespace detail {

CloudConnectVersion clientCloudConnectVersion()
{
    if (test::kCurrentCloudConnectVersionOverride)
        return *test::kCurrentCloudConnectVersionOverride;

    return kCurrentCloudConnectVersion;
}

} // namespace detail

namespace stun = network::stun;
using namespace stun::extension;

ConnectRequest::ConnectRequest():
    StunRequestData(kMethod)
{
}

void ConnectRequest::serializeAttributes(nx::network::stun::Message* const message)
{
    message->newAttribute<attrs::HostName>(std::move(destinationHostName));
    message->newAttribute<attrs::PeerId>(std::move(originatingPeerId));
    message->newAttribute<attrs::ConnectionId>(connectSessionId);
    message->newAttribute<attrs::ConnectionMethods>(std::to_string(connectionMethods));
    message->newAttribute<attrs::UdtHpEndpointList>(std::move(udpEndpointList));
    message->addAttribute(attrs::ignoreSourceAddress, ignoreSourceAddress);
    message->addAttribute(attrs::cloudConnectVersion, (int)cloudConnectVersion);
}

bool ConnectRequest::parseAttributes(const nx::network::stun::Message& message)
{
    if (!readEnumAttributeValue(message, attrs::cloudConnectVersion, &cloudConnectVersion))
        cloudConnectVersion = kDefaultCloudConnectVersion;  //if not present - old version

    return
        readStringAttributeValue<attrs::HostName>(message, &destinationHostName) &&
        readStringAttributeValue<attrs::PeerId>(message, &originatingPeerId) &&
        readStringAttributeValue<attrs::ConnectionId>(message, &connectSessionId) &&
        readIntAttributeValue<attrs::ConnectionMethods>(message, &connectionMethods) &&
        readAttributeValue<attrs::UdtHpEndpointList>(message, &udpEndpointList) &&
        readAttributeValue(message, attrs::ignoreSourceAddress, &ignoreSourceAddress);
}

//-------------------------------------------------------------------------------------------------
// ConnectResponse

ConnectResponse::ConnectResponse():
    StunResponseData(kMethod)
{
}

void ConnectResponse::serializeAttributes(nx::network::stun::Message* const message)
{
    message->newAttribute<attrs::PublicEndpointList>(std::move(forwardedTcpEndpointList));
    message->newAttribute<attrs::UdtHpEndpointList>(std::move(udpEndpointList));

    // Adding attrs::TrafficRelayUrl for backward compatibility.
    if (trafficRelayUrl)
        message->newAttribute<attrs::TrafficRelayUrl>(*trafficRelayUrl);

    message->newAttribute<attrs::StringList>(
        attrs::trafficRelayUrlList,
        trafficRelayUrls);

    message->newAttribute<attrs::HostName>(destinationHostFullName);
    params.serializeAttributes(message);
    message->addAttribute(attrs::cloudConnectVersion, (int)cloudConnectVersion);

    if (alternateMediatorEndpointStunUdp)
        message->newAttribute<stun::attrs::AlternateServer>(*alternateMediatorEndpointStunUdp);
}

bool ConnectResponse::parseAttributes(const nx::network::stun::Message& message)
{
    if (!readEnumAttributeValue(message, attrs::cloudConnectVersion, &cloudConnectVersion))
        cloudConnectVersion = kDefaultCloudConnectVersion;  //if not present - old version

    // Backward compatibility with an old mediator.
    std::string trafficRelayUrlLocal;
    if (readStringAttributeValue<attrs::TrafficRelayUrl>(message, &trafficRelayUrlLocal))
        trafficRelayUrl = trafficRelayUrlLocal;

    readAttributeValue<attrs::StringList>(
        message,
        attrs::trafficRelayUrlList,
        &trafficRelayUrls);

    readStringAttributeValue<attrs::HostName>(message, &destinationHostFullName);

    network::SocketAddress mediatorEndpoint;
    if (readAttributeValue<stun::attrs::AlternateServer>(message, &mediatorEndpoint))
        alternateMediatorEndpointStunUdp = std::move(mediatorEndpoint);

    return
        readAttributeValue<attrs::PublicEndpointList>(message, &forwardedTcpEndpointList) &&
        readAttributeValue<attrs::UdtHpEndpointList>(message, &udpEndpointList) &&
        params.parseAttributes(message);
}

} // namespace nx::hpm::api
