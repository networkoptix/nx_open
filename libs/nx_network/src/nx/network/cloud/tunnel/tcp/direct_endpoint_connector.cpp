// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "direct_endpoint_connector.h"

#include <nx/utils/log/log.h>

#include "direct_endpoint_tunnel.h"
#include "tunnel_tcp_endpoint_verificator_factory.h"

namespace nx::network::cloud::tcp {

DirectEndpointConnector::DirectEndpointConnector(
    AddressEntry targetHostAddress,
    std::string connectSessionId)
    :
    m_targetHostAddress(std::move(targetHostAddress)),
    m_connectSessionId(std::move(connectSessionId))
{
    bindToAioThread(getAioThread());
}

void DirectEndpointConnector::bindToAioThread(
    aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    for (auto& verificator: m_verificators)
        verificator->bindToAioThread(aioThread);
}

int DirectEndpointConnector::getPriority() const
{
    // TODO: #akolesnikov
    return 0;
}

void DirectEndpointConnector::connect(
    const hpm::api::ConnectResponse& response,
    std::chrono::milliseconds timeout,
    ConnectCompletionHandler handler)
{
    NX_VERBOSE(this, "%1. Connecting to %2 with timeout %3",
        m_connectSessionId, m_targetHostAddress, timeout);

    NX_ASSERT(!response.forwardedTcpEndpointList.empty());
    if (!s_needVerification)
    {
        auto endpoint = response.forwardedTcpEndpointList.front();

        NX_VERBOSE(this, "%1. Verification is disabled. Reporting endpoint %2",
            m_connectSessionId, endpoint);

        return post(
            [this, endpoint = std::move(endpoint), handler = std::move(handler)]() mutable
            {
                m_completionHandler = std::move(handler);
                reportSuccessfulVerificationResult(std::move(endpoint), nullptr);
            });
    }

    performEndpointVerification(
        response.forwardedTcpEndpointList,
        timeout,
        std::move(handler));
}

const AddressEntry& DirectEndpointConnector::targetPeerAddress() const
{
    return m_targetHostAddress;
}

void DirectEndpointConnector::setVerificationRequirement(bool value)
{
    s_needVerification = value;
}

void DirectEndpointConnector::stopWhileInAioThread()
{
    m_verificators.clear();
}

bool DirectEndpointConnector::s_needVerification(true);

void DirectEndpointConnector::performEndpointVerification(
    std::vector<SocketAddress> endpoints,
    std::chrono::milliseconds timeout,
    ConnectCompletionHandler handler)
{
    post(
        [this, endpoints = std::move(endpoints), timeout, handler = std::move(handler)]() mutable
        {
            removeInvalidEmptyAddresses(&endpoints);

            if (endpoints.empty())
            {
                NX_WARNING(this, "%1. target %2. Cannot verify empty address list",
                    m_connectSessionId, m_targetHostAddress);
                handler(
                    nx::hpm::api::NatTraversalResultCode::tcpConnectFailed,
                    SystemError::connectionReset,
                    nullptr);
                return;
            }

            m_completionHandler = std::move(handler);

            launchVerificators(endpoints, timeout);
        });
}

void DirectEndpointConnector::removeInvalidEmptyAddresses(
    std::vector<SocketAddress>* endpoints)
{
    for (auto it = endpoints->begin(); it != endpoints->end();)
    {
        if (it->address.toString().empty())
        {
            NX_WARNING(this, "%1. target %2. Received empty address",
                m_connectSessionId, m_targetHostAddress);
            it = endpoints->erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void DirectEndpointConnector::launchVerificators(
    const std::vector<SocketAddress>& endpoints,
    std::chrono::milliseconds timeout)
{
    using namespace std::placeholders;

    for (const SocketAddress& endpoint: endpoints)
    {
        NX_VERBOSE(this, "%1. Verifying host %2", m_connectSessionId, endpoint);

        m_verificators.push_back(
            EndpointVerificatorFactory::instance().create(m_connectSessionId));
        m_verificators.back()->setTimeout(timeout);
        m_verificators.back()->verifyHost(
            endpoint,
            m_targetHostAddress,
            std::bind(&DirectEndpointConnector::onVerificationDone, this,
                endpoint, --m_verificators.end(), _1));
    }
}

void DirectEndpointConnector::onVerificationDone(
    const SocketAddress& endpoint,
    Verificators::iterator verificatorIter,
    AbstractEndpointVerificator::VerificationResult verificationResult)
{
    auto verificator = std::move(*verificatorIter);
    m_verificators.erase(verificatorIter);

    switch (verificationResult)
    {
        case AbstractEndpointVerificator::VerificationResult::passed:
            reportSuccessfulVerificationResult(
                endpoint,
                verificator->takeSocket());
            break;

        case AbstractEndpointVerificator::VerificationResult::ioError:
            return reportErrorOnEndpointVerificationFailure(
                nx::hpm::api::NatTraversalResultCode::tcpConnectFailed,
                verificator->lastSystemErrorCode());
            break;

        case AbstractEndpointVerificator::VerificationResult::notPassed:
            return reportErrorOnEndpointVerificationFailure(
                nx::hpm::api::NatTraversalResultCode::endpointVerificationFailure,
                SystemError::noError);
            break;
    }
}

void DirectEndpointConnector::reportErrorOnEndpointVerificationFailure(
    nx::hpm::api::NatTraversalResultCode resultCode,
    SystemError::ErrorCode sysErrorCode)
{
    if (!m_verificators.empty())
        return;
    auto handler = std::move(m_completionHandler);
    m_completionHandler = nullptr;
    return handler(
        resultCode,
        sysErrorCode,
        nullptr);
}

void DirectEndpointConnector::reportSuccessfulVerificationResult(
    SocketAddress endpoint,
    std::unique_ptr<AbstractStreamSocket> streamSocket)
{
    NX_VERBOSE(this, "%1. Reporting successful connection to %2", m_connectSessionId, endpoint);

    m_verificators.clear();

    auto tunnel = std::make_unique<DirectTcpEndpointTunnel>(
        getAioThread(),
        m_connectSessionId,
        std::move(endpoint),
        std::move(streamSocket));

    auto handler = std::move(m_completionHandler);
    m_completionHandler = nullptr;
    handler(
        nx::hpm::api::NatTraversalResultCode::ok,
        SystemError::noError,
        std::move(tunnel));
}

} // namespace nx::network::cloud::tcp
