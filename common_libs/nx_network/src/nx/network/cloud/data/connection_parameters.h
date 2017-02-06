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
    kDefaultTunnelInactivityTimeout = std::chrono::minutes(8);

/**
 * @note All fields are optional for backward compatibility.
 */
class NX_NETWORK_API ConnectionParameters:
    public StunMessageAttributesData
{
public:
    std::chrono::milliseconds rendezvousConnectTimeout;
    std::chrono::milliseconds udpTunnelKeepAliveInterval;
    /**
     * UDP tunnel should be considered inactive if no keep-alive messages have been 
     *   received during rendezvousConnectTimeout*udpTunnelKeepAliveRetries period.
     */
    int udpTunnelKeepAliveRetries;
    std::chrono::seconds tunnelInactivityTimeout;

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
