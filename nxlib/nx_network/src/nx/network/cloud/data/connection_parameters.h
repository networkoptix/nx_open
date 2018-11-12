#pragma once

#include <chrono>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/retry_timer.h>
#include <nx/network/deprecated/asynchttpclient.h>

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

// Nat traversal methods default start delays.

constexpr static const std::chrono::milliseconds
    kUdpHolePunchingStartDelayDefault = std::chrono::milliseconds::zero();

constexpr static const std::chrono::milliseconds
    kTrafficRelayingStartDelayDefault = std::chrono::seconds(2);

constexpr static const std::chrono::milliseconds
    kDirectTcpConnectStartDelayDefault = std::chrono::milliseconds::zero();

/**
 * NOTE: All fields are optional for backward compatibility.
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
    nx::network::http::AsyncHttpClient::Timeouts tcpReverseHttpTimeouts;

    std::chrono::milliseconds udpHolePunchingStartDelay;
    std::chrono::milliseconds trafficRelayingStartDelay;
    std::chrono::milliseconds directTcpConnectStartDelay;

    ConnectionParameters();
    bool operator==(const ConnectionParameters& rhs) const;

    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

#define ConnectionParameters_Fields \
    (rendezvousConnectTimeout)(udpTunnelKeepAliveInterval)(udpTunnelKeepAliveRetries) \
    (tunnelInactivityTimeout)/*(tcpReverseRetryPolicy)(tcpReverseHttpTimeouts)*/ \
    (udpHolePunchingStartDelay)(trafficRelayingStartDelay)(directTcpConnectStartDelay)

QN_FUSION_DECLARE_FUNCTIONS(ConnectionParameters, (json), NX_NETWORK_API)

} // namespace api
} // namespace hpm
} // namespace nx
