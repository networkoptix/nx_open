#include "get_connection_state_data.h"

namespace nx {
namespace hpm {
namespace api {

GetConnectionStateRequest::GetConnectionStateRequest():
    StunRequestData(kMethod)
{
}

void GetConnectionStateRequest::serializeAttributes(nx::stun::Message* const /*message*/)
{
}

bool GetConnectionStateRequest::parseAttributes(const nx::stun::Message& /*message*/)
{
    return true;
}

//-------------------------------------------------------------------------------------------------

GetConnectionStateResponse::GetConnectionStateResponse():
    StunResponseData(kMethod)
{
}

void GetConnectionStateResponse::serializeAttributes(nx::stun::Message* const message)
{
    message->addAttribute(stun::extension::attrs::isListening, (int) state);
}

bool GetConnectionStateResponse::parseAttributes(const nx::stun::Message& message)
{
    return readAttributeValue(message, stun::extension::attrs::isListening, (int*) &state);
}

} // namespace api
} // namespace hpm
} // namespace nx
