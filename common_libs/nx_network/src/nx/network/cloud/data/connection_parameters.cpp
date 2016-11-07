/**********************************************************
* Apr 18, 2016
* akolesnikov
***********************************************************/

#include "connection_parameters.h"

namespace nx {
namespace hpm {
namespace api {

ConnectionParameters::ConnectionParameters()
:
    rendezvousConnectTimeout(kRendezvousConnectTimeoutDefault),
    udpTunnelKeepAliveInterval(kUdpTunnelKeepAliveIntervalDefault),
    udpTunnelKeepAliveRetries(kUdpTunnelKeepAliveRetriesDefault),
    crossNatTunnelInactivityTimeout(kCrossNatTunnelInactivityTimeoutDefault)
{
}

bool ConnectionParameters::operator==(const ConnectionParameters& rhs) const
{
    return rendezvousConnectTimeout == rhs.rendezvousConnectTimeout
        && udpTunnelKeepAliveInterval == rhs.udpTunnelKeepAliveInterval
        && udpTunnelKeepAliveRetries == rhs.udpTunnelKeepAliveRetries
        && crossNatTunnelInactivityTimeout == rhs.crossNatTunnelInactivityTimeout
        && tcpReverseRetryPolicy == rhs.tcpReverseRetryPolicy
        && tcpReverseHttpTimeouts == rhs.tcpReverseHttpTimeouts;
}

void ConnectionParameters::serializeAttributes(nx::stun::Message* const message)
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
    message->addAttribute(
        stun::cc::attrs::crossNatTunnelInactivityTimeout,
        crossNatTunnelInactivityTimeout);

    message->addAttribute(
        stun::cc::attrs::tcpReverseRetryMaxCount,
        (int)tcpReverseRetryPolicy.maxRetryCount());
    message->addAttribute(
        stun::cc::attrs::tcpReverseRetryInitialDelay,
        tcpReverseRetryPolicy.initialDelay());
    message->addAttribute(
        stun::cc::attrs::tcpReverseRetryDelayMultiplier,
        (int)tcpReverseRetryPolicy.delayMultiplier());
    message->addAttribute(
        stun::cc::attrs::tcpReverseRetryMaxDelay,
        tcpReverseRetryPolicy.maxDelay());

    message->addAttribute(
        stun::cc::attrs::tcpReverseHttpSendTimeout,
        tcpReverseHttpTimeouts.sendTimeout);
    message->addAttribute(
        stun::cc::attrs::tcpReverseHttpReadTimeout,
        tcpReverseHttpTimeouts.responseReadTimeout);
    message->addAttribute(
        stun::cc::attrs::tcpReverseHttpMsgBodyTimeout,
        tcpReverseHttpTimeouts.messageBodyReadTimeout);
}

bool ConnectionParameters::parseAttributes(const nx::stun::Message& message)
{
    // All attributes are optional.

    readAttributeValue(
        message, stun::cc::attrs::rendezvousConnectTimeout,
        &rendezvousConnectTimeout,
        (std::chrono::milliseconds)kRendezvousConnectTimeoutDefault);
    readAttributeValue(
        message, stun::cc::attrs::udpTunnelKeepAliveInterval,
        &udpTunnelKeepAliveInterval,
        (std::chrono::milliseconds)kUdpTunnelKeepAliveIntervalDefault);
    readAttributeValue(
        message, stun::cc::attrs::udpTunnelKeepAliveRetries,
        &udpTunnelKeepAliveRetries,
        (int)kUdpTunnelKeepAliveRetriesDefault);
    readAttributeValue(
        message, stun::cc::attrs::crossNatTunnelInactivityTimeout,
        &crossNatTunnelInactivityTimeout,
        (std::chrono::seconds)kCrossNatTunnelInactivityTimeoutDefault);

    // TODO: make real support for unsigned int(s) in STUN,
    //  curently unsigned int(s) are represented like simple ints
    readAttributeValue(
        message, stun::cc::attrs::tcpReverseRetryMaxCount,
        (int*)&tcpReverseRetryPolicy.m_maxRetryCount,
        (int)network::RetryPolicy::kDefaultMaxRetryCount);
    readAttributeValue(
        message, stun::cc::attrs::tcpReverseRetryInitialDelay,
        &tcpReverseRetryPolicy.m_initialDelay,
        (std::chrono::milliseconds)network::RetryPolicy::kDefaultInitialDelay);
    readAttributeValue(
        message, stun::cc::attrs::tcpReverseRetryDelayMultiplier,
        (int*)&tcpReverseRetryPolicy.m_delayMultiplier,
        (int)network::RetryPolicy::kDefaultDelayMultiplier);
    readAttributeValue(
        message, stun::cc::attrs::tcpReverseRetryMaxDelay,
        &tcpReverseRetryPolicy.m_maxDelay,
        (std::chrono::milliseconds)network::RetryPolicy::kDefaultMaxDelay);

    readAttributeValue(
        message, stun::cc::attrs::tcpReverseHttpSendTimeout,
        &tcpReverseHttpTimeouts.sendTimeout,
        (std::chrono::milliseconds)nx_http::AsyncHttpClient::Timeouts::kDefaultSendTimeout);
    readAttributeValue(
        message, stun::cc::attrs::tcpReverseHttpReadTimeout,
        &tcpReverseHttpTimeouts.responseReadTimeout,
        (std::chrono::milliseconds)nx_http::AsyncHttpClient::Timeouts::kDefaultResponseReadTimeout);
    readAttributeValue(
        message, stun::cc::attrs::tcpReverseHttpMsgBodyTimeout,
        &tcpReverseHttpTimeouts.messageBodyReadTimeout,
        (std::chrono::milliseconds)nx_http::AsyncHttpClient::Timeouts::kDefaultMessageBodyReadTimeout);

    return true;
}

} // namespace api
} // namespace hpm
} // namespace nx
