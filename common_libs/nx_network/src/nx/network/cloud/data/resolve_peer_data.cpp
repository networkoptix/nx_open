#include "resolve_peer_data.h"

#include <nx/network/stun/cc/custom_stun.h>


namespace nx {
namespace hpm {
namespace api {

ResolvePeerRequest::ResolvePeerRequest()
:
    ResolvePeerRequest(nx::String())
{
}

ResolvePeerRequest::ResolvePeerRequest(nx::String _hostName)
:
    StunRequestData(kMethod),
    hostName(std::move(_hostName))
{
}

void ResolvePeerRequest::serializeAttributes(nx::stun::Message* const message)
{
    message->newAttribute<stun::cc::attrs::HostName>(hostName);
}

bool ResolvePeerRequest::parseAttributes(const nx::stun::Message& message)
{
    return readStringAttributeValue<stun::cc::attrs::HostName>(message, &hostName);
}


ResolvePeerResponse::ResolvePeerResponse()
:
    StunResponseData(kMethod),
    connectionMethods(0)
{
}

void ResolvePeerResponse::serializeAttributes(nx::stun::Message* const message)
{
    message->newAttribute< stun::cc::attrs::PublicEndpointList >(std::move(endpoints));
    message->newAttribute< stun::cc::attrs::ConnectionMethods >(
        nx::String::number(static_cast<qulonglong>(connectionMethods)));
}

bool ResolvePeerResponse::parseAttributes(const nx::stun::Message& message)
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

} // namespace api
} // namespace hpm
} // namespace nx
