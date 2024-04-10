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

    for (auto& verificatorCtx: m_verificators)
        verificatorCtx.verificator->bindToAioThread(aioThread);
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
                reportSuccessfulVerificationResult({.endpoint = endpoint});
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
                return handler({
                    .resultCode = nx::hpm::api::NatTraversalResultCode::tcpConnectFailed,
                    .sysErrorCode = SystemError::connectionReset});
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
    for (const SocketAddress& endpoint: endpoints)
    {
        NX_VERBOSE(this, "%1. Verifying host %2", m_connectSessionId, endpoint);

        auto& ctx = m_verificators.emplace_back();
        ctx.endpoint = endpoint;
        ctx.verificator = EndpointVerificatorFactory::instance().create(m_connectSessionId);
        ctx.stats.remoteAddress = endpoint.toString();
        ctx.stats.connectType = ConnectType::forwardedTcpPort;

        ctx.verificator->setTimeout(timeout);
        ctx.reponseTimer.restart();

        ctx.verificator->verifyHost(
            endpoint,
            m_targetHostAddress,
            std::bind(&DirectEndpointConnector::onVerificationDone, this,
                --m_verificators.end(), std::placeholders::_1));
    }
}

void DirectEndpointConnector::onVerificationDone(
    Verificators::iterator verificatorIter,
    AbstractEndpointVerificator::VerificationResult verificationResult)
{
    verificatorIter->stats.responseTime = verificatorIter->reponseTimer.elapsed();

    auto verificatorCtx = std::move(*verificatorIter);
    m_verificators.erase(verificatorIter);

    switch (verificationResult)
    {
        case AbstractEndpointVerificator::VerificationResult::passed:
            reportSuccessfulVerificationResult(std::move(verificatorCtx));
            break;

        case AbstractEndpointVerificator::VerificationResult::ioError:
        {
            const auto sysErrorCode = verificatorCtx.verificator->lastSystemErrorCode();
            return reportErrorOnEndpointVerificationFailure(
                nx::hpm::api::NatTraversalResultCode::tcpConnectFailed,
                sysErrorCode,
                std::move(verificatorCtx));
            break;
        }
        case AbstractEndpointVerificator::VerificationResult::notPassed:
            return reportErrorOnEndpointVerificationFailure(
                nx::hpm::api::NatTraversalResultCode::endpointVerificationFailure,
                SystemError::noError,
                std::move(verificatorCtx));
            break;
    }
}

void DirectEndpointConnector::reportErrorOnEndpointVerificationFailure(
    nx::hpm::api::NatTraversalResultCode resultCode,
    SystemError::ErrorCode sysErrorCode,
    VerificatorContext verificatorCtx)
{
    if (!m_verificators.empty())
        return;

    TunnelConnectResult error{
        .resultCode = resultCode,
        .sysErrorCode = sysErrorCode,
        .stats = std::move(verificatorCtx.stats)};

    nx::utils::swapAndCall(m_completionHandler, std::move(error));
}

void DirectEndpointConnector::reportSuccessfulVerificationResult(VerificatorContext verificatorCtx)
{
    NX_VERBOSE(this, "%1. Reporting successful connection to %2",
        m_connectSessionId, verificatorCtx.endpoint);

    auto socket = verificatorCtx.verificator ? verificatorCtx.verificator->takeSocket() : nullptr;

    TunnelConnectResult result {
        .resultCode = nx::hpm::api::NatTraversalResultCode::ok,
        .sysErrorCode = SystemError::noError,
        .connection = std::make_unique<DirectTcpEndpointTunnel>(
            getAioThread(),
            m_connectSessionId,
            std::move(verificatorCtx.endpoint),
            std::move(socket)),
        .stats = std::move(verificatorCtx.stats),
    };

    m_verificators.clear();

    nx::utils::swapAndCall(m_completionHandler, std::move(result));
}

} // namespace nx::network::cloud::tcp
