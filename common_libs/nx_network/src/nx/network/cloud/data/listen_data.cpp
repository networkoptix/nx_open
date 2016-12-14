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
    message->newAttribute<stun::cc::attrs::SystemId>(systemId);
    message->newAttribute<stun::cc::attrs::ServerId>(serverId);
    message->addAttribute(stun::cc::attrs::cloudConnectVersion, (int)cloudConnectVersion);
}

bool ListenRequest::parseAttributes(const nx::stun::Message& message)
{
    if (!readEnumAttributeValue(message, stun::cc::attrs::cloudConnectVersion, &cloudConnectVersion))
        cloudConnectVersion = kDefaultCloudConnectVersion; //< If not present - old version.

    return
        readStringAttributeValue<stun::cc::attrs::SystemId>(message, &systemId) &&
        readStringAttributeValue<stun::cc::attrs::ServerId>(message, &serverId);
}

ListenResponse::ListenResponse():
    StunResponseData(kMethod)
{
}

typedef stun::cc::attrs::StringAttribute<stun::cc::attrs::tcpConnectionKeepAlive>
    TcpConnectionKeepAlive;

void ListenResponse::serializeAttributes(nx::stun::Message* const message)
{
    if (tcpConnectionKeepAlive)
        message->newAttribute<TcpConnectionKeepAlive>(tcpConnectionKeepAlive->toString().toUtf8());
}

bool ListenResponse::parseAttributes(const nx::stun::Message& message)
{
    nx::String keepAliveOptions;
    if (readStringAttributeValue<TcpConnectionKeepAlive>(message, &keepAliveOptions))
    {
        tcpConnectionKeepAlive = KeepAliveOptions::fromString(QString::fromUtf8(keepAliveOptions));
        return (bool)tcpConnectionKeepAlive; //< Empty means parsing has failed.
    }
    else
    {
        tcpConnectionKeepAlive = boost::none;
    }

    return true;
}

} // namespace api
} // namespace hpm
} // namespace nx
