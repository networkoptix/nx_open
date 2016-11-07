/**********************************************************
* Apr 18, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <chrono>

#include <nx/network/retry_timer.h>
#include <nx/network/http/asynchttpclient.h>

#include "stun_message_data.h"

namespace nx {
namespace hpm {
namespace api {

constexpr static const std::chrono::seconds
    kRendezvousConnectTimeoutDefault = std::chrono::seconds(15);
constexpr static const std::chrono::seconds
    kUdpTunnelKeepAliveIntervalDefault = std::chrono::seconds(15);
constexpr static const size_t kUdpTunnelKeepAliveRetriesDefault = 3;
constexpr static const std::chrono::seconds 
    kCrossNatTunnelInactivityTimeoutDefault = std::chrono::minutes(8);

/**
 * @note All fields are optional for backwartd compatibility.
 */
class NX_NETWORK_API ConnectionParameters
:
    public StunMessageAttributesData
{
public:
    std::chrono::milliseconds rendezvousConnectTimeout;
    std::chrono::milliseconds udpTunnelKeepAliveInterval;
    /**
     * UDP tunnel should be considered inactive if no keep-alive messages have been 
     * received during \a rendezvousConnectTimeout*udpTunnelKeepAliveRetries period.
     */
    int udpTunnelKeepAliveRetries;
    std::chrono::seconds crossNatTunnelInactivityTimeout;

    nx::network::RetryPolicy tcpReverseRetryPolicy;
    nx_http::AsyncHttpClient::Timeouts tcpReverseHttpTimeouts;

    ConnectionParameters();
    bool operator==(const ConnectionParameters& rhs) const;

    virtual void serializeAttributes(nx::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::stun::Message& message) override;
};

} // namespace api
} // namespace hpm
} // namespace nx
