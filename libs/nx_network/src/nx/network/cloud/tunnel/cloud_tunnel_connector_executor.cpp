// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_tunnel_connector_executor.h"

#include <nx/utils/log/log.h>

namespace nx::network::cloud {

using namespace nx::hpm;

ConnectorExecutor::ConnectorExecutor(
    const AddressEntry& targetAddress,
    const std::string& connectSessionId,
    const hpm::api::ConnectResponse& response,
    std::unique_ptr<AbstractDatagramSocket> udpSocket)
    :
    m_connectSessionId(connectSessionId),
    m_response(response)
{
    auto connectors = ConnectorFactory::instance().create(
        targetAddress,
        connectSessionId,
        response,
        std::move(udpSocket));

    for (auto& connector: connectors)
    {
        auto& connectorCtx = m_connectors.emplace_back();
        connectorCtx.tunnelCtx = std::move(connector);
        connectorCtx.startDelayTimer = std::make_unique<aio::Timer>();
    }

    bindToAioThread(getAioThread());
}

void ConnectorExecutor::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    for (auto& ctx: m_connectors)
    {
        ctx.tunnelCtx.connector->bindToAioThread(aioThread);
        ctx.startDelayTimer->bindToAioThread(aioThread);
    }
}

void ConnectorExecutor::setTimeout(std::chrono::milliseconds timeout)
{
    m_connectTimeout = timeout;
}

void ConnectorExecutor::start(CompletionHandler handler)
{
    dispatch(
        [this, handler = std::move(handler)]() mutable
        {
            m_handler = std::move(handler);

            if (m_connectors.empty())
                return reportNoSuitableConnectMethod();

            NX_VERBOSE(this, nx::format("cross-nat %1. Starting %2 connectors")
                .arg(m_connectSessionId).arg(m_connectors.size()));

            auto connectorWithMinDelayIter = std::min_element(
                m_connectors.begin(), m_connectors.end(),
                [](const ConnectorContext& left, const ConnectorContext& right)
                {
                    return left.tunnelCtx.startDelay < right.tunnelCtx.startDelay;
                });
            const auto minDelay = connectorWithMinDelayIter->tunnelCtx.startDelay;

            for (auto& connectorContext: m_connectors)
                connectorContext.tunnelCtx.startDelay -= minDelay;

            startConnectors();
        });
}

void ConnectorExecutor::stopWhileInAioThread()
{
    m_connectors.clear();
}

void ConnectorExecutor::reportNoSuitableConnectMethod()
{
    post(
        [this]()
        {
            TunnelConnectResult connectResult {
                .resultCode = nx::hpm::api::NatTraversalResultCode::noSuitableMethod,
                .sysErrorCode = SystemError::hostUnreachable,
            };
            nx::utils::swapAndCall(m_handler, std::move(connectResult));
        });
}

void ConnectorExecutor::startConnectors()
{
    for (auto it = m_connectors.begin(); it != m_connectors.end(); ++it)
    {
        if (it->tunnelCtx.startDelay > std::chrono::milliseconds::zero())
        {
            it->startDelayTimer->start(
                it->tunnelCtx.startDelay,
                std::bind(&ConnectorExecutor::startConnector, this, it));
        }
        else
        {
            startConnector(it);
        }
    }
}

void ConnectorExecutor::startConnector(
    std::list<ConnectorContext>::iterator connectorIter)
{
    connectorIter->tunnelCtx.connector->connect(
        m_response,
        m_connectTimeout,
        [this, connectorIter](auto&& ... args)
        {
            onConnectorFinished(std::move(connectorIter), std::forward<decltype(args)>(args)...);
        });
}

void ConnectorExecutor::onConnectorFinished(
    std::list<ConnectorContext>::iterator connectorIter,
    TunnelConnectResult result)
{
    NX_VERBOSE(this, "cross-nat %1. Connector has finished with result: %2, %3",
        m_connectSessionId, result.resultCode, result.sysErrorCode);

    auto connector = std::move(*connectorIter);
    m_connectors.erase(connectorIter);

    if (!result.ok() && !m_connectors.empty())
        return;     // Waiting for other connectors to complete.

    NX_CRITICAL(!result.ok() || result.connection);
    m_connectors.clear();   // Cancelling other connectors.

    nx::utils::swapAndCall(m_handler, std::move(result));
}

} // namespace nx::network::cloud
