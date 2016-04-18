/**********************************************************
* Apr 18, 2016
* akolesnikov
***********************************************************/

#include "connection_parameters.h"


namespace nx {
namespace hpm {
namespace api {

constexpr static const std::chrono::milliseconds
    kRendezvousConnectTimeoutDefault = std::chrono::seconds(15);
constexpr static const std::chrono::milliseconds
    kUdpTunnelKeepAliveIntervalDefault = std::chrono::seconds(15);
constexpr static const size_t kUdpTunnelKeepAliveRetriesDefault = 3;

ConnectionParameters::ConnectionParameters()
:
    rendezvousConnectTimeout(kRendezvousConnectTimeoutDefault),
    udpTunnelKeepAliveInterval(kUdpTunnelKeepAliveIntervalDefault),
    udpTunnelKeepAliveRetries(kUdpTunnelKeepAliveRetriesDefault)
{
}

void ConnectionParameters::serialize(nx::stun::Message* const message)
{
    message->addAttribute(
        stun::cc::attrs::rendezvousConnectTimeout,
        rendezvousConnectTimeout);
    message->addAttribute(
        stun::cc::attrs::udpTunnelKeepAliveInterval,
        udpTunnelKeepAliveInterval);
    message->addAttribute(
        stun::cc::attrs::udpTunnelKeepAliveRetries,
        udpTunnelKeepAliveRetries);
}

bool ConnectionParameters::parse(const nx::stun::Message& message)
{
    //all attributes are optional

    rendezvousConnectTimeout = kRendezvousConnectTimeoutDefault;
    readAttributeValue(
        message,
        stun::cc::attrs::rendezvousConnectTimeout,
        &rendezvousConnectTimeout);

    udpTunnelKeepAliveInterval = kUdpTunnelKeepAliveIntervalDefault;
    readAttributeValue(
        message,
        stun::cc::attrs::udpTunnelKeepAliveInterval,
        &udpTunnelKeepAliveInterval);

    udpTunnelKeepAliveRetries = kUdpTunnelKeepAliveRetriesDefault;
    readAttributeValue(
        message,
        stun::cc::attrs::udpTunnelKeepAliveRetries,
        &udpTunnelKeepAliveRetries);

    return true;
}

}   //api
}   //hpm
}   //nx
