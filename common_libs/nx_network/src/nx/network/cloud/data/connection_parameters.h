/**********************************************************
* Apr 18, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <chrono>

#include "stun_message_data.h"


namespace nx {
namespace hpm {
namespace api {

constexpr static const std::chrono::seconds
    kRendezvousConnectTimeoutDefault = std::chrono::seconds(15);
constexpr static const std::chrono::seconds
    kUdpTunnelKeepAliveIntervalDefault = std::chrono::seconds(15);
constexpr static const size_t kUdpTunnelKeepAliveRetriesDefault = 3;

/**
    \note All fields are optional for backwartd compatibility
*/
class NX_NETWORK_API ConnectionParameters
:
    public StunMessageData
{
public:
    std::chrono::milliseconds rendezvousConnectTimeout;
    std::chrono::milliseconds udpTunnelKeepAliveInterval;
    /** UDP tunnel should be considered inactive if no keep-alive messages have been 
        received during \a rendezvousConnectTimeout*udpTunnelKeepAliveRetries period
    */
    int udpTunnelKeepAliveRetries;

    ConnectionParameters();

    void serialize(nx::stun::Message* const message);
    bool parse(const nx::stun::Message& message);
};

}   //api
}   //hpm
}   //nx
