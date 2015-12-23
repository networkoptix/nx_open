/**********************************************************
* Dec 22, 2015
* akolesnikov
***********************************************************/

#include "resolve_data.h"

#include <nx/network/stun/cc/custom_stun.h>

namespace nx {
namespace network {
namespace cloud {
namespace api {

ResolveRequest::ResolveRequest()
{
}

ResolveRequest::ResolveRequest(nx::String _hostName)
:
    hostName(std::move(_hostName))
{
}

void ResolveRequest::serialize(nx::stun::Message* const message)
{
    message->newAttribute<stun::cc::attrs::HostName>(hostName);
}

bool ResolveRequest::parse(const nx::stun::Message& message)
{
    const auto hostNameAttr = message.getAttribute< stun::cc::attrs::HostName >();
    if (!hostNameAttr)
    {
        setErrorText("Missing required attribute HostName");
        return false;
    }
    hostName = hostNameAttr->getString();
    return true;
}


ResolveResponse::ResolveResponse()
:
    connectionMethods(0)
{
}

void ResolveResponse::serialize(nx::stun::Message* const message)
{
    message->newAttribute< stun::cc::attrs::PublicEndpointList >(std::move(endpoints));
    message->newAttribute< stun::cc::attrs::ConnectionMethods >(
        nx::String::number(static_cast<qulonglong>(connectionMethods)));
}

bool ResolveResponse::parse(const nx::stun::Message& message)
{
    const auto endpointsAttr = message.getAttribute< stun::cc::attrs::PublicEndpointList >();
    if (!endpointsAttr)
    {
        setErrorText("Missing required attribute PublicEndpointList");
        return false;
    }
    endpoints = endpointsAttr->get();

    const auto connectionMethodsAttr = message.getAttribute< stun::cc::attrs::ConnectionMethods >();
    if (!connectionMethodsAttr)
    {
        setErrorText("Missing required attribute ConnectionMethods");
        return false;
    }
    connectionMethods = connectionMethodsAttr->getString().toLongLong();

    return true;
}

} // namespace api
} // namespace cloud
} // namespace network
} // namespace nx
