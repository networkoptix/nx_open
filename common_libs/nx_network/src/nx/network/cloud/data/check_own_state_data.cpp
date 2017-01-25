#include "check_own_state_data.h"

namespace nx {
namespace hpm {
namespace api {

CheckOwnStateRequest::CheckOwnStateRequest():
    StunRequestData(kMethod)
{
}

void CheckOwnStateRequest::serializeAttributes(nx::stun::Message* const /*message*/)
{
}

bool CheckOwnStateRequest::parseAttributes(const nx::stun::Message& /*message*/)
{
    return true;
}

CheckOwnStateResponse::CheckOwnStateResponse():
    StunResponseData(kMethod)
{
}

void CheckOwnStateResponse::serializeAttributes(nx::stun::Message* const message)
{
    message->addAttribute(stun::extension::attrs::isListening, isListening);
}

bool CheckOwnStateResponse::parseAttributes(const nx::stun::Message& message)
{
    return readAttributeValue(message, stun::extension::attrs::isListening, &isListening);
}

} // namespace api
} // namespace hpm
} // namespace nx
