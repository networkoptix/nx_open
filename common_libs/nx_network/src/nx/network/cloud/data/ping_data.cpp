
#include "ping_data.h"


namespace nx {
namespace hpm {
namespace api {

PingRequest::PingRequest()
{
}

PingRequest::PingRequest(std::list<SocketAddress> _endpoints)
:
    endpoints(std::move(_endpoints))
{
}

void PingRequest::serialize(nx::stun::Message* const message)
{
    message->newAttribute< stun::cc::attrs::PublicEndpointList >(
        std::move(endpoints));
}

bool PingRequest::parse(const nx::stun::Message& message)
{
    return readAttributeValue<stun::cc::attrs::PublicEndpointList>(
        message, &endpoints);
}


void PingResponse::serialize(nx::stun::Message* const message)
{
    message->newAttribute< stun::cc::attrs::PublicEndpointList >(
        std::move(endpoints));
}

bool PingResponse::parse(const nx::stun::Message& message)
{
    return readAttributeValue<stun::cc::attrs::PublicEndpointList>(
        message, &endpoints);
}

}   //api
}   //hpm
}   //nx
