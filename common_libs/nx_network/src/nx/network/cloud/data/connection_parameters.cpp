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
    tunnelInactivityTimeout(kDefaultTunnelInactivityTimeout)
{
}

bool ConnectionParameters::operator==(const ConnectionParameters& rhs) const
{
    return rendezvousConnectTimeout == rhs.rendezvousConnectTimeout
        && udpTunnelKeepAliveInterval == rhs.udpTunnelKeepAliveInterval
        && udpTunnelKeepAliveRetries == rhs.udpTunnelKeepAliveRetries
        && tunnelInactivityTimeout == rhs.tunnelInactivityTimeout
        && tcpReverseRetryPolicy == rhs.tcpReverseRetryPolicy
        && tcpReverseHttpTimeouts == rhs.tcpReverseHttpTimeouts;
}

void ConnectionParameters::serializeAttributes(nx::stun::Message* const message)
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
        (int)tcpReverseRetryPolicy.maxRetryCount());
    message->addAttribute(
        stun::extension::attrs::tcpReverseRetryInitialDelay,
        tcpReverseRetryPolicy.initialDelay());
    message->addAttribute(
        stun::extension::attrs::tcpReverseRetryDelayMultiplier,
        (int)tcpReverseRetryPolicy.delayMultiplier());
    message->addAttribute(
        stun::extension::attrs::tcpReverseRetryMaxDelay,
        tcpReverseRetryPolicy.maxDelay());

    message->addAttribute(
        stun::extension::attrs::tcpReverseHttpSendTimeout,
        tcpReverseHttpTimeouts.sendTimeout);
    message->addAttribute(
        stun::extension::attrs::tcpReverseHttpReadTimeout,
        tcpReverseHttpTimeouts.responseReadTimeout);
    message->addAttribute(
        stun::extension::attrs::tcpReverseHttpMsgBodyTimeout,
        tcpReverseHttpTimeouts.messageBodyReadTimeout);
}

bool ConnectionParameters::parseAttributes(const nx::stun::Message& message)
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
        (int*)&tcpReverseRetryPolicy.m_maxRetryCount,
        (int)network::RetryPolicy::kDefaultMaxRetryCount);
    readAttributeValue(
        message, stun::extension::attrs::tcpReverseRetryInitialDelay,
        &tcpReverseRetryPolicy.m_initialDelay,
        (std::chrono::milliseconds)network::RetryPolicy::kDefaultInitialDelay);
    readAttributeValue(
        message, stun::extension::attrs::tcpReverseRetryDelayMultiplier,
        (int*)&tcpReverseRetryPolicy.m_delayMultiplier,
        (int)network::RetryPolicy::kDefaultDelayMultiplier);
    readAttributeValue(
        message, stun::extension::attrs::tcpReverseRetryMaxDelay,
        &tcpReverseRetryPolicy.m_maxDelay,
        (std::chrono::milliseconds)network::RetryPolicy::kDefaultMaxDelay);

    readAttributeValue(
        message, stun::extension::attrs::tcpReverseHttpSendTimeout,
        &tcpReverseHttpTimeouts.sendTimeout,
        (std::chrono::milliseconds)nx_http::AsyncHttpClient::Timeouts::kDefaultSendTimeout);
    readAttributeValue(
        message, stun::extension::attrs::tcpReverseHttpReadTimeout,
        &tcpReverseHttpTimeouts.responseReadTimeout,
        (std::chrono::milliseconds)nx_http::AsyncHttpClient::Timeouts::kDefaultResponseReadTimeout);
    readAttributeValue(
        message, stun::extension::attrs::tcpReverseHttpMsgBodyTimeout,
        &tcpReverseHttpTimeouts.messageBodyReadTimeout,
        (std::chrono::milliseconds)nx_http::AsyncHttpClient::Timeouts::kDefaultMessageBodyReadTimeout);

    return true;
}

} // namespace api
} // namespace hpm
} // namespace nx
