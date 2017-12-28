#include "listen_data.h"

namespace nx {
namespace hpm {
namespace api {

ListenRequest::ListenRequest():
    StunRequestData(kMethod),
    cloudConnectVersion(kCurrentCloudConnectVersion)
{
}

void ListenRequest::serializeAttributes(nx::network::stun::Message* const message)
{
    message->newAttribute<stun::extension::attrs::SystemId>(systemId);
    message->newAttribute<stun::extension::attrs::ServerId>(serverId);
    message->addAttribute(stun::extension::attrs::cloudConnectVersion, (int)cloudConnectVersion);
}

bool ListenRequest::parseAttributes(const nx::network::stun::Message& message)
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

void ListenResponse::serializeAttributes(nx::network::stun::Message* const message)
{
    using namespace stun::extension;

    if (tcpConnectionKeepAlive)
        message->newAttribute<TcpKeepAlive>(tcpConnectionKeepAlive->toString().toUtf8());

    message->addAttribute(attrs::cloudConnectOptions, (int) cloudConnectOptions);
    if (trafficRelayUrl)
        message->newAttribute<attrs::TrafficRelayUrl>(std::move(*trafficRelayUrl));
    message->newAttribute<attrs::StringList>(
        nx::network::stun::extension::attrs::trafficRelayUrlList,
        std::move(trafficRelayUrls));
}

bool ListenResponse::parseAttributes(const nx::network::stun::Message& message)
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

    nx::String trafficRelayUrlLocal;
    if (readStringAttributeValue<attrs::TrafficRelayUrl>(message, &trafficRelayUrlLocal))
        trafficRelayUrl = std::move(trafficRelayUrlLocal);

    readAttributeValue<attrs::StringList>(
        message,
        attrs::trafficRelayUrlList,
        &trafficRelayUrls);

    if (trafficRelayUrl && trafficRelayUrls.empty())
    {
        // For compatibility between internal 3.1 builds.
        // TODO: #ak Remove in 3.2.
        trafficRelayUrls.push_back(std::move(*trafficRelayUrl));
        trafficRelayUrl.reset();
    }

    return true;
}

} // namespace api
} // namespace hpm
} // namespace nx
