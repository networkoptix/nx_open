// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "listen_data.h"

namespace nx::hpm::api {

ListenRequest::ListenRequest():
    StunRequestData(kMethod)
{
}

void ListenRequest::serializeAttributes(nx::network::stun::Message* const message)
{
    using namespace nx::network;

    message->newAttribute<stun::extension::attrs::SystemId>(systemId);
    message->newAttribute<stun::extension::attrs::ServerId>(serverId);
    message->addAttribute(stun::extension::attrs::cloudConnectVersion, (int)cloudConnectVersion);
}

bool ListenRequest::parseAttributes(const nx::network::stun::Message& message)
{
    using namespace nx::network;

    if (!readEnumAttributeValue(
            message,
            stun::extension::attrs::cloudConnectVersion,
            &cloudConnectVersion))
    {
        cloudConnectVersion = kDefaultCloudConnectVersion; //< If not present - old version.
    }

    return
        readStringAttributeValue<stun::extension::attrs::SystemId>(message, &systemId) &&
        readStringAttributeValue<stun::extension::attrs::ServerId>(message, &serverId);
}

//-------------------------------------------------------------------------------------------------

ListenResponse::ListenResponse():
    StunResponseData(kMethod)
{
}

using TcpKeepAlive =
    network::stun::extension::attrs::StringAttribute<network::stun::extension::attrs::tcpConnectionKeepAlive>;

void ListenResponse::serializeAttributes(nx::network::stun::Message* const message)
{
    using namespace nx::network;
    using namespace nx::network::stun::extension;

    if (tcpConnectionKeepAlive)
        message->newAttribute<TcpKeepAlive>(tcpConnectionKeepAlive->toString());

    message->addAttribute(attrs::cloudConnectOptions, (int) cloudConnectOptions);
    if (trafficRelayUrl)
        message->newAttribute<attrs::TrafficRelayUrl>(std::move(*trafficRelayUrl));
    message->newAttribute<attrs::StringList>(
        attrs::trafficRelayUrlList,
        std::move(trafficRelayUrls));

    message->addAttribute(
        attrs::trafficRelayConnectTimeout,
        trafficRelayConnectTimeout);
}

bool ListenResponse::parseAttributes(const nx::network::stun::Message& message)
{
    using namespace nx::network;
    using namespace nx::network::stun::extension;

    tcpConnectionKeepAlive = std::nullopt;
    std::string keepAliveOptions;
    if (readStringAttributeValue<TcpKeepAlive>(message, &keepAliveOptions))
    {
        tcpConnectionKeepAlive =
            network::KeepAliveOptions::fromString(keepAliveOptions);
        if (!tcpConnectionKeepAlive)
            return false; //< Empty means parsing has failed.
    }

    if (!readEnumAttributeValue(message, attrs::cloudConnectOptions, &cloudConnectOptions))
        cloudConnectOptions = {};

    std::string trafficRelayUrlLocal;
    if (readStringAttributeValue<attrs::TrafficRelayUrl>(message, &trafficRelayUrlLocal))
        trafficRelayUrl = std::move(trafficRelayUrlLocal);

    readAttributeValue<attrs::StringList>(
        message,
        attrs::trafficRelayUrlList,
        &trafficRelayUrls);

    if (trafficRelayUrl && trafficRelayUrls.empty())
    {
        // For compatibility between internal 3.1 builds.
        // TODO: #akolesnikov Remove in 3.2.
        trafficRelayUrls.push_back(std::move(*trafficRelayUrl));
        trafficRelayUrl.reset();
    }

    readAttributeValue(
        message,
        attrs::trafficRelayConnectTimeout,
        &trafficRelayConnectTimeout);

    return true;
}

} // namespace nx::hpm::api
