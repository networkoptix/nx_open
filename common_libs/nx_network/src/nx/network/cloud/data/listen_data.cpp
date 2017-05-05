#include "listen_data.h"

namespace nx {
namespace hpm {
namespace api {

ListenRequest::ListenRequest():
    StunRequestData(kMethod),
    cloudConnectVersion(kCurrentCloudConnectVersion)
{
}

void ListenRequest::serializeAttributes(nx::stun::Message* const message)
{
    message->newAttribute<stun::extension::attrs::SystemId>(systemId);
    message->newAttribute<stun::extension::attrs::ServerId>(serverId);
    message->addAttribute(stun::extension::attrs::cloudConnectVersion, (int)cloudConnectVersion);
}

bool ListenRequest::parseAttributes(const nx::stun::Message& message)
{
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
    StunResponseData(kMethod),
    cloudConnectOptions(emptyCloudConnectOptions)
{
}

using TcpKeepAlive = 
    stun::extension::attrs::StringAttribute<stun::extension::attrs::tcpConnectionKeepAlive>;

void ListenResponse::serializeAttributes(nx::stun::Message* const message)
{
    using namespace stun::extension;

    if (tcpConnectionKeepAlive)
        message->newAttribute<TcpKeepAlive>(tcpConnectionKeepAlive->toString().toUtf8());

    message->addAttribute(attrs::cloudConnectOptions, (int) cloudConnectOptions);
    if (trafficRelayEndpoint)
        message->newAttribute<attrs::TrafficRelayEndpoint>(std::move(*trafficRelayEndpoint));
}

bool ListenResponse::parseAttributes(const nx::stun::Message& message)
{
    using namespace stun::extension;

    tcpConnectionKeepAlive = boost::none;
    nx::String keepAliveOptions;
    if (readStringAttributeValue<TcpKeepAlive>(message, &keepAliveOptions))
    {
        tcpConnectionKeepAlive = KeepAliveOptions::fromString(QString::fromUtf8(keepAliveOptions));
        if (!tcpConnectionKeepAlive)
            return false; //< Empty means parsing has failed.
    }

    if (!readEnumAttributeValue(message, attrs::cloudConnectOptions, &cloudConnectOptions))
        cloudConnectOptions = emptyCloudConnectOptions;

    SocketAddress trafficRelayEndpointLocal;
    if (readAttributeValue<attrs::TrafficRelayEndpoint>(message, &trafficRelayEndpointLocal))
        trafficRelayEndpoint = std::move(trafficRelayEndpointLocal);

    return true;
}

} // namespace api
} // namespace hpm
} // namespace nx
