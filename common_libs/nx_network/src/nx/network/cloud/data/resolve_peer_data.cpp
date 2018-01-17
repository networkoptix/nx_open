#include "resolve_peer_data.h"

#include <nx/network/stun/extension/stun_extension_types.h>

namespace nx {
namespace hpm {
namespace api {

ResolvePeerRequest::ResolvePeerRequest(nx::String _hostName):
    StunRequestData(kMethod),
    hostName(std::move(_hostName))
{
}

void ResolvePeerRequest::serializeAttributes(nx::network::stun::Message* const message)
{
    message->newAttribute<network::stun::extension::attrs::HostName>(hostName);
}

bool ResolvePeerRequest::parseAttributes(const nx::network::stun::Message& message)
{
    return readStringAttributeValue<network::stun::extension::attrs::HostName>(message, &hostName);
}

ResolvePeerResponse::ResolvePeerResponse():
    StunResponseData(kMethod),
    connectionMethods(0)
{
}

void ResolvePeerResponse::serializeAttributes(nx::network::stun::Message* const message)
{
    message->newAttribute<network::stun::extension::attrs::PublicEndpointList >(std::move(endpoints));
    message->newAttribute<network::stun::extension::attrs::ConnectionMethods >(
        nx::String::number(static_cast<qulonglong>(connectionMethods)));
}

bool ResolvePeerResponse::parseAttributes(const nx::network::stun::Message& message)
{
    if (!readAttributeValue<network::stun::extension::attrs::PublicEndpointList>(message, &endpoints))
        return false;
    nx::String connectionMethodsStr;
    if (!readStringAttributeValue<network::stun::extension::attrs::ConnectionMethods>(
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
