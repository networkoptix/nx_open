// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connection_parameters.h"

#include <nx/fusion/model_functions.h>

namespace nx::hpm::api {

using namespace nx::network;

ConnectionParameters::ConnectionParameters():
    rendezvousConnectTimeout(kRendezvousConnectTimeoutDefault),
    udpTunnelKeepAliveInterval(kUdpTunnelKeepAliveIntervalDefault),
    udpTunnelKeepAliveRetries(kUdpTunnelKeepAliveRetriesDefault),
    tunnelInactivityTimeout(kDefaultTunnelInactivityTimeout),
    udpHolePunchingStartDelay(kUdpHolePunchingStartDelayDefault),
    trafficRelayingStartDelay(kTrafficRelayingStartDelayDefault),
    directTcpConnectStartDelay(kDirectTcpConnectStartDelayDefault)
{
}

bool ConnectionParameters::operator==(const ConnectionParameters& rhs) const
{
    return rendezvousConnectTimeout == rhs.rendezvousConnectTimeout
        && udpTunnelKeepAliveInterval == rhs.udpTunnelKeepAliveInterval
        && udpTunnelKeepAliveRetries == rhs.udpTunnelKeepAliveRetries
        && tunnelInactivityTimeout == rhs.tunnelInactivityTimeout
        && tcpReverseRetryPolicy == rhs.tcpReverseRetryPolicy
        && tcpReverseHttpTimeouts == rhs.tcpReverseHttpTimeouts
        && udpHolePunchingStartDelay == rhs.udpHolePunchingStartDelay
        && trafficRelayingStartDelay == rhs.trafficRelayingStartDelay
        && directTcpConnectStartDelay == rhs.directTcpConnectStartDelay;
}

void ConnectionParameters::serializeAttributes(nx::network::stun::Message* const message)
{
    message->addAttribute(
        stun::extension::attrs::rendezvousConnectTimeout,
        rendezvousConnectTimeout);
    message->addAttribute(
        stun::extension::attrs::udpTunnelKeepAliveInterval,
        udpTunnelKeepAliveInterval);
    message->addAttribute(
        stun::extension::attrs::udpTunnelKeepAliveRetries,
        udpTunnelKeepAliveRetries);
    message->addAttribute(
        stun::extension::attrs::tunnelInactivityTimeout,
        tunnelInactivityTimeout);

    message->addAttribute(
        stun::extension::attrs::tcpReverseRetryMaxCount,
        (int)tcpReverseRetryPolicy.maxRetryCount);
    message->addAttribute(
        stun::extension::attrs::tcpReverseRetryInitialDelay,
        tcpReverseRetryPolicy.initialDelay);
    message->addAttribute(
        stun::extension::attrs::tcpReverseRetryDelayMultiplier,
        (int)tcpReverseRetryPolicy.delayMultiplier);
    message->addAttribute(
        stun::extension::attrs::tcpReverseRetryMaxDelay,
        tcpReverseRetryPolicy.maxDelay);

    message->addAttribute(
        stun::extension::attrs::tcpReverseHttpSendTimeout,
        tcpReverseHttpTimeouts.sendTimeout);
    message->addAttribute(
        stun::extension::attrs::tcpReverseHttpReadTimeout,
        tcpReverseHttpTimeouts.responseReadTimeout);
    message->addAttribute(
        stun::extension::attrs::tcpReverseHttpMsgBodyTimeout,
        tcpReverseHttpTimeouts.messageBodyReadTimeout);

    // Start delays.

    message->addAttribute(
        stun::extension::attrs::udpHolePunchingStartDelay,
        udpHolePunchingStartDelay);
    message->addAttribute(
        stun::extension::attrs::trafficRelayingStartDelay,
        trafficRelayingStartDelay);
    message->addAttribute(
        stun::extension::attrs::directTcpConnectStartDelay,
        directTcpConnectStartDelay);
}

bool ConnectionParameters::parseAttributes(const nx::network::stun::Message& message)
{
    // All attributes are optional.

    readAttributeValue(
        message, stun::extension::attrs::rendezvousConnectTimeout,
        &rendezvousConnectTimeout,
        (std::chrono::milliseconds)kRendezvousConnectTimeoutDefault);
    readAttributeValue(
        message, stun::extension::attrs::udpTunnelKeepAliveInterval,
        &udpTunnelKeepAliveInterval,
        (std::chrono::milliseconds)kUdpTunnelKeepAliveIntervalDefault);
    readAttributeValue(
        message, stun::extension::attrs::udpTunnelKeepAliveRetries,
        &udpTunnelKeepAliveRetries,
        (int)kUdpTunnelKeepAliveRetriesDefault);
    readAttributeValue(
        message, stun::extension::attrs::tunnelInactivityTimeout,
        &tunnelInactivityTimeout,
        std::chrono::duration_cast<std::chrono::seconds>(kDefaultTunnelInactivityTimeout));

    // TODO: make real support for unsigned int(s) in STUN,
    //  curently unsigned int(s) are represented like simple ints
    readAttributeValue(
        message, stun::extension::attrs::tcpReverseRetryMaxCount,
        (int*)&tcpReverseRetryPolicy.maxRetryCount,
        (int)network::RetryPolicy::kDefaultMaxRetryCount);
    readAttributeValue(
        message, stun::extension::attrs::tcpReverseRetryInitialDelay,
        &tcpReverseRetryPolicy.initialDelay,
        (std::chrono::milliseconds)network::RetryPolicy::kDefaultInitialDelay);
    readAttributeValue(
        message, stun::extension::attrs::tcpReverseRetryDelayMultiplier,
        (int*)&tcpReverseRetryPolicy.delayMultiplier,
        (int)network::RetryPolicy::kDefaultDelayMultiplier);
    readAttributeValue(
        message, stun::extension::attrs::tcpReverseRetryMaxDelay,
        &tcpReverseRetryPolicy.maxDelay,
        (std::chrono::milliseconds)network::RetryPolicy::kDefaultMaxDelay);

    readAttributeValue(
        message, stun::extension::attrs::tcpReverseHttpSendTimeout,
        &tcpReverseHttpTimeouts.sendTimeout,
        (std::chrono::milliseconds)nx::network::http::AsyncClient::Timeouts::defaults().sendTimeout);
    readAttributeValue(
        message, stun::extension::attrs::tcpReverseHttpReadTimeout,
        &tcpReverseHttpTimeouts.responseReadTimeout,
        (std::chrono::milliseconds)nx::network::http::AsyncClient::Timeouts::defaults().responseReadTimeout);
    readAttributeValue(
        message, stun::extension::attrs::tcpReverseHttpMsgBodyTimeout,
        &tcpReverseHttpTimeouts.messageBodyReadTimeout,
        (std::chrono::milliseconds)nx::network::http::AsyncClient::Timeouts::defaults().messageBodyReadTimeout);

    // Start delays.

    readAttributeValue(
        message, stun::extension::attrs::udpHolePunchingStartDelay,
        &udpHolePunchingStartDelay,
        std::chrono::milliseconds::zero());
    readAttributeValue(
        message, stun::extension::attrs::trafficRelayingStartDelay,
        &trafficRelayingStartDelay,
        std::chrono::milliseconds::zero());
    readAttributeValue(
        message, stun::extension::attrs::directTcpConnectStartDelay,
        &directTcpConnectStartDelay,
        std::chrono::milliseconds::zero());

    return true;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ConnectionParameters, (json), ConnectionParameters_Fields)

} // namespace nx::hpm::api
