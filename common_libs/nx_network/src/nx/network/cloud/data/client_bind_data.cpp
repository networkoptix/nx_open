#include "client_bind_data.h"

namespace nx {
namespace hpm {
namespace api {

ClientBindRequest::ClientBindRequest():
    StunRequestData(kMethod)
{
}

void ClientBindRequest::serializeAttributes(nx::stun::Message* const message)
{
    message->newAttribute<stun::cc::attrs::PeerId>(std::move(originatingPeerID));
    message->newAttribute<stun::cc::attrs::TcpReverseEndpointList>(std::move(tcpReverseEndpoints));
}

bool ClientBindRequest::parseAttributes(const nx::stun::Message& message)
{
    return
        readStringAttributeValue<stun::cc::attrs::PeerId>(message, &originatingPeerID) &&
        readAttributeValue<stun::cc::attrs::TcpReverseEndpointList>(message, &tcpReverseEndpoints);

}

} // namespace api
} // namespace hpm
} // namespace nx
