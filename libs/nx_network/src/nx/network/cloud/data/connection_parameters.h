// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/retry_timer.h>
#include <nx/reflect/instrument.h>

#include "stun_message_data.h"

namespace nx::hpm::api {

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
struct NX_NETWORK_API ConnectionParameters:
    StunMessageAttributesData
{
    /**%apidoc Timeout for establishing a connect between peers using UDP hole punching. */
    std::chrono::milliseconds rendezvousConnectTimeout = kRendezvousConnectTimeoutDefault;

    std::chrono::milliseconds udpTunnelKeepAliveInterval = kUdpTunnelKeepAliveIntervalDefault;

    /**%apidoc UDP tunnel should be considered inactive if no keep-alive messages have been
     * received during rendezvousConnectTimeout*udpTunnelKeepAliveRetries period.
     */
    int udpTunnelKeepAliveRetries = kUdpTunnelKeepAliveRetriesDefault;

    /**%apidoc A period specifying for how long a logical link between client and server should
     * be preserved (by preserving UDP hole punching ports or server's relays).
     */
    std::chrono::seconds tunnelInactivityTimeout = kDefaultTunnelInactivityTimeout;

    nx::network::RetryPolicy tcpReverseRetryPolicy;
    nx::network::http::AsyncHttpClient::Timeouts tcpReverseHttpTimeouts;

    /**%apidoc Delay the client should respect before doing UDP hole punching after receiving
     * this response.
     */
    std::chrono::milliseconds udpHolePunchingStartDelay = kUdpHolePunchingStartDelayDefault;

    /**%apidoc Delay the client should respect before doing connect via Cloud relay after receiving
     * this response.
     */
    std::chrono::milliseconds trafficRelayingStartDelay = kTrafficRelayingStartDelayDefault;

    /**%apidoc Delay the client should respect before connecting to the peer's public TCP
     * endpoint(s) (if any).
     */
    std::chrono::milliseconds directTcpConnectStartDelay = kDirectTcpConnectStartDelayDefault;

    bool operator==(const ConnectionParameters& rhs) const;

    virtual void serializeAttributes(nx::network::stun::Message* const message) override;
    virtual bool parseAttributes(const nx::network::stun::Message& message) override;
};

#define ConnectionParameters_Fields \
    (rendezvousConnectTimeout)(udpTunnelKeepAliveInterval)(udpTunnelKeepAliveRetries) \
    (tunnelInactivityTimeout)/*(tcpReverseRetryPolicy)(tcpReverseHttpTimeouts)*/ \
    (udpHolePunchingStartDelay)(trafficRelayingStartDelay)(directTcpConnectStartDelay)

NX_REFLECTION_INSTRUMENT(ConnectionParameters, ConnectionParameters_Fields)

} // namespace nx::hpm::api
