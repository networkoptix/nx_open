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
    StunResponseData(kMethod),
    cloudConnectOptions(emptyCloudConnectOptions)
{
}

typedef stun::extension::attrs::StringAttribute<stun::extension::attrs::tcpConnectionKeepAlive> TcpKeepAlive;

void ListenResponse::serializeAttributes(nx::stun::Message* const message)
{
    if (tcpConnectionKeepAlive)
        message->newAttribute<TcpKeepAlive>(tcpConnectionKeepAlive->toString().toUtf8());

    message->addAttribute(stun::extension::attrs::cloudConnectOptions, (int) cloudConnectOptions);
}

bool ListenResponse::parseAttributes(const nx::stun::Message& message)
{
    tcpConnectionKeepAlive = boost::none;
    nx::String keepAliveOptions;
    if (readStringAttributeValue<TcpKeepAlive>(message, &keepAliveOptions))
    {
        tcpConnectionKeepAlive = KeepAliveOptions::fromString(QString::fromUtf8(keepAliveOptions));
        if (!tcpConnectionKeepAlive)
            return false; //< Empty means parsing has failed.
    }

    if (!readEnumAttributeValue(message, stun::extension::attrs::cloudConnectOptions, &cloudConnectOptions))
        cloudConnectOptions = emptyCloudConnectOptions;

    return true;
}

} // namespace api
} // namespace hpm
} // namespace nx
