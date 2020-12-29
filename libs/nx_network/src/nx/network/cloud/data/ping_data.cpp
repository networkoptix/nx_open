#include "ping_data.h"

namespace nx {
namespace hpm {
namespace api {

PingRequest::PingRequest():
    PingRequest(std::list<network::SocketAddress>())
{
}

PingRequest::PingRequest(std::list<network::SocketAddress> _endpoints):
    StunRequestData(kMethod),
    endpoints(std::move(_endpoints))
{
}

void PingRequest::serializeAttributes(nx::network::stun::Message* const message)
{
    message->newAttribute< network::stun::extension::attrs::PublicEndpointList >(
        std::move(endpoints));
}

bool PingRequest::parseAttributes(const nx::network::stun::Message& message)
{
    return readAttributeValue<network::stun::extension::attrs::PublicEndpointList>(
        message, &endpoints);
}

//-------------------------------------------------------------------------------------------------

PingResponse::PingResponse():
    StunResponseData(kMethod)
{
}

void PingResponse::serializeAttributes(nx::network::stun::Message* const message)
{
    message->newAttribute<network::stun::extension::attrs::PublicEndpointList >(
        std::move(endpoints));
}

bool PingResponse::parseAttributes(const nx::network::stun::Message& message)
{
    return readAttributeValue<network::stun::extension::attrs::PublicEndpointList>(
        message, &endpoints);
}

} // namespace api
} // namespace hpm
} // namespace nx
