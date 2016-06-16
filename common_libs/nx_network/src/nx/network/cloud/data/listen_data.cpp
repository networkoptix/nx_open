/**********************************************************
* Jan 13, 2016
* akolesnikov
***********************************************************/

#include "listen_data.h"


namespace nx {
namespace hpm {
namespace api {

ListenRequest::ListenRequest()
:
    StunRequestData(kMethod),
    cloudConnectVersion(kCurrentCloudConnectVersion)
{
}

void ListenRequest::serializeAttributes(nx::stun::Message* const message)
{
    message->newAttribute<stun::cc::attrs::SystemId>(systemId);
    message->newAttribute<stun::cc::attrs::ServerId>(serverId);
    message->addAttribute(stun::cc::attrs::cloudConnectVersion, (int)cloudConnectVersion);
}

bool ListenRequest::parseAttributes(const nx::stun::Message& message)
{
    if (!readEnumAttributeValue(message, stun::cc::attrs::cloudConnectVersion, &cloudConnectVersion))
        cloudConnectVersion = kDefaultCloudConnectVersion;  //if not present - old version

    return
        readStringAttributeValue<stun::cc::attrs::SystemId>(message, &systemId) &&
        readStringAttributeValue<stun::cc::attrs::ServerId>(message, &serverId);
}

} // namespace api
} // namespace hpm
} // namespace nx
