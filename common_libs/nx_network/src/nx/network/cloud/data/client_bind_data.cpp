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

ClientBindResponse::ClientBindResponse():
    StunResponseData(kMethod)
{
}

typedef stun::cc::attrs::StringAttribute<stun::cc::attrs::tcpConnectionKeepAlive> TcpKeepAlive;

void ClientBindResponse::serializeAttributes(nx::stun::Message* const message)
{
    if (tcpConnectionKeepAlive)
        message->newAttribute<TcpKeepAlive>(tcpConnectionKeepAlive->toString().toUtf8());
}

bool ClientBindResponse::parseAttributes(const nx::stun::Message& message)
{
    nx::String keepAliveOptions;
    if (readStringAttributeValue<TcpKeepAlive>(message, &keepAliveOptions))
    {
        tcpConnectionKeepAlive = KeepAliveOptions::fromString(QString::fromUtf8(keepAliveOptions));
        return (bool)tcpConnectionKeepAlive; //< Empty means parsing has failed.
    }
    else
    {
        tcpConnectionKeepAlive = boost::none;
    }

    return true;
}

} // namespace api
} // namespace hpm
} // namespace nx
