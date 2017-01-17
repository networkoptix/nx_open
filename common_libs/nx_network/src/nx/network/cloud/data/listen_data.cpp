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
    if (!readEnumAttributeValue(message, stun::extension::attrs::cloudConnectVersion, &cloudConnectVersion))
        cloudConnectVersion = kDefaultCloudConnectVersion; //< If not present - old version.

    return
        readStringAttributeValue<stun::extension::attrs::SystemId>(message, &systemId) &&
        readStringAttributeValue<stun::extension::attrs::ServerId>(message, &serverId);
}

ListenResponse::ListenResponse():
    StunResponseData(kMethod)
{
}

typedef stun::extension::attrs::StringAttribute<stun::extension::attrs::tcpConnectionKeepAlive> TcpKeepAlive;

void ListenResponse::serializeAttributes(nx::stun::Message* const message)
{
    if (tcpConnectionKeepAlive)
        message->newAttribute<TcpKeepAlive>(tcpConnectionKeepAlive->toString().toUtf8());
}

bool ListenResponse::parseAttributes(const nx::stun::Message& message)
{
    nx::String keepAliveOptions;
    if (readStringAttributeValue<TcpKeepAlive>(message, &keepAliveOptions))
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
