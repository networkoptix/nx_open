/**********************************************************
* Dec 22, 2015
* akolesnikov
***********************************************************/

#include "resolve_data.h"

#include <nx/network/stun/cc/custom_stun.h>


namespace nx {
namespace cc {
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
    return readStringAttributeValue<stun::cc::attrs::HostName>(message, &hostName);
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
        nx::String::number(connectionMethods));
}

bool ResolveResponse::parse(const nx::stun::Message& message)
{
    if (!readAttributeValue<stun::cc::attrs::PublicEndpointList>(message, &endpoints))
        return false;
    nx::String connectionMethodsStr;
    if (!readStringAttributeValue<stun::cc::attrs::ConnectionMethods>(
            message, &connectionMethodsStr))
    {
        return false;
    }
    connectionMethods = connectionMethodsStr.toLongLong();

    return true;
}

}   //api
}   //cc
}   //nx
