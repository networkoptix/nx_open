/**********************************************************
* Jan 13, 2016
* akolesnikov
***********************************************************/

#include "listen_data.h"


namespace nx {
namespace hpm {
namespace api {

void ListenRequest::serialize(nx::stun::Message* const message)
{
    message->newAttribute<stun::cc::attrs::SystemId>(systemID);
    message->newAttribute<stun::cc::attrs::ServerId>(serverID);
}

bool ListenRequest::parse(const nx::stun::Message& message)
{
    return 
        readStringAttributeValue<stun::cc::attrs::SystemId>(message, &systemID) &&
        readStringAttributeValue<stun::cc::attrs::ServerId>(message, &serverID);
}

} // namespace api
} // namespace hpm
} // namespace nx
