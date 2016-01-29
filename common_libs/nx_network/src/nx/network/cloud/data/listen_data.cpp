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
    message->newAttribute<stun::cc::attrs::SystemId>(systemId);
    message->newAttribute<stun::cc::attrs::ServerId>(serverId);
}

bool ListenRequest::parse(const nx::stun::Message& message)
{
    return 
        readStringAttributeValue<stun::cc::attrs::SystemId>(message, &systemId) &&
        readStringAttributeValue<stun::cc::attrs::ServerId>(message, &serverId);
}

} // namespace api
} // namespace hpm
} // namespace nx
