#include "client_bind_data.h"

namespace nx {
namespace hpm {
namespace api {

using namespace nx::network;

ClientBindRequest::ClientBindRequest():
    StunRequestData(kMethod)
{
}

void ClientBindRequest::serializeAttributes(nx::network::stun::Message* const message)
{
    message->newAttribute<stun::extension::attrs::PeerId>(
        std::move(originatingPeerID));
    message->newAttribute<stun::extension::attrs::TcpReverseEndpointList>(
        std::move(tcpReverseEndpoints));
}

bool ClientBindRequest::parseAttributes(const nx::network::stun::Message& message)
{
    return
        readStringAttributeValue<stun::extension::attrs::PeerId>(
            message, &originatingPeerID) &&
        readAttributeValue<stun::extension::attrs::TcpReverseEndpointList>(
            message, &tcpReverseEndpoints);

}

//-------------------------------------------------------------------------------------------------

ClientBindResponse::ClientBindResponse():
    StunResponseData(kMethod)
{
}

typedef stun::extension::attrs::StringAttribute<
    stun::extension::attrs::tcpConnectionKeepAlive
> TcpKeepAlive;

void ClientBindResponse::serializeAttributes(nx::network::stun::Message* const message)
{
    if (tcpConnectionKeepAlive)
        message->newAttribute<TcpKeepAlive>(tcpConnectionKeepAlive->toString().toUtf8());
}

bool ClientBindResponse::parseAttributes(const nx::network::stun::Message& message)
{
    nx::String keepAliveOptions;
    if (readStringAttributeValue<TcpKeepAlive>(message, &keepAliveOptions))
    {
        tcpConnectionKeepAlive =
            KeepAliveOptions::fromString(QString::fromUtf8(keepAliveOptions));
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
