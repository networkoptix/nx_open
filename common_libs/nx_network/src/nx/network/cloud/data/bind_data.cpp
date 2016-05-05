
#include "bind_data.h"


namespace nx {
namespace hpm {
namespace api {

BindRequest::BindRequest()
{
}

BindRequest::BindRequest(std::list<SocketAddress> _publicEndpoints)
:
    publicEndpoints(std::move(_publicEndpoints))
{
}

void BindRequest::serialize(nx::stun::Message* const message)
{
    message->newAttribute< stun::cc::attrs::PublicEndpointList >(
        std::move(publicEndpoints));
}

bool BindRequest::parse(const nx::stun::Message& message)
{
    return readAttributeValue<stun::cc::attrs::PublicEndpointList>(
        message, &publicEndpoints);
}

}   //api
}   //hpm
}   //nx
