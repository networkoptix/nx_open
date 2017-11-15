#include "bind_data.h"

namespace nx {
namespace hpm {
namespace api {

BindRequest::BindRequest(std::list<SocketAddress> _publicEndpoints):
    StunRequestData(kMethod),
    publicEndpoints(std::move(_publicEndpoints))
{
}

void BindRequest::serializeAttributes(nx::stun::Message* const message)
{
    message->newAttribute< stun::extension::attrs::PublicEndpointList >(
        std::move(publicEndpoints));
}

bool BindRequest::parseAttributes(const nx::stun::Message& message)
{
    return readAttributeValue<stun::extension::attrs::PublicEndpointList>(
        message, &publicEndpoints);
}

//-------------------------------------------------------------------------------------------------

BindResponse::BindResponse():
    StunResponseData(kMethod)
{
}

void BindResponse::serializeAttributes(nx::stun::Message* const /*message*/)
{
}

bool BindResponse::parseAttributes(const nx::stun::Message& /*message*/)
{
    return true;
}

} // namespace api
} // namespace hpm
} // namespace nx
