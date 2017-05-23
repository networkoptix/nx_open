#include "cloud_tunnel_connector_executor.h"

#include <nx/fusion/serialization/lexical.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace network {
namespace cloud {

using namespace nx::hpm;

ConnectorExecutor::ConnectorExecutor(
    const AddressEntry& targetAddress,
    const nx::String& connectSessionId,
    const hpm::api::ConnectResponse& response,
    std::unique_ptr<UDPSocket> udpSocket)
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
        ConnectorContext context;
        context.connector = std::move(connector.connector);
        context.startDelay = connector.startDelay;
        context.timer = std::make_unique<aio::Timer>();
        m_connectors.emplace_back(std::move(context));
    }

    bindToAioThread(getAioThread());
}

void ConnectorExecutor::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    for (auto& connectorContext: m_connectors)
    {
        connectorContext.connector->bindToAioThread(aioThread);
        connectorContext.timer->bindToAioThread(aioThread);
    }
}

void ConnectorExecutor::setTimeout(std::chrono::milliseconds timeout)
{
    m_connectTimeout = timeout;
}

void ConnectorExecutor::start(CompletionHandler handler)
{
    m_handler = std::move(handler);

    if (m_connectors.empty())
        return reportNoSuitableConnectMethod();

    NX_LOGX(lm("cross-nat %1. Starting %2 connector(s)")
        .arg(m_connectSessionId).arg(m_connectors.size()),
        cl_logDEBUG2);

    auto connectorWithMinDelayIter = std::min_element(
        m_connectors.begin(), m_connectors.end(),
        [](const ConnectorContext& left, const ConnectorContext& right)
        {
            return left.startDelay < right.startDelay;
        });
    const auto delayOffset = connectorWithMinDelayIter->startDelay;

    for (auto it = m_connectors.begin(); it != m_connectors.end(); ++it)
    {
        if (it->startDelay > std::chrono::milliseconds::zero())
        {
            it->timer->start(
                it->startDelay - delayOffset,
                std::bind(&ConnectorExecutor::startConnector, this, it));
        }
        else
        {
            startConnector(it);
        }
    }
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
            nx::utils::swapAndCall(
                m_handler,
                nx::hpm::api::NatTraversalResultCode::noSuitableMethod,
                SystemError::hostUnreach,
                nullptr);
        });
}

void ConnectorExecutor::startConnector(
    std::list<ConnectorContext>::iterator connectorIter)
{
    using namespace std::placeholders;

    connectorIter->connector->connect(
        m_response,
        m_connectTimeout,
        std::bind(&ConnectorExecutor::onConnectorFinished, this,
            connectorIter, _1, _2, _3));
}

void ConnectorExecutor::onConnectorFinished(
    std::list<ConnectorContext>::iterator connectorIter,
    nx::hpm::api::NatTraversalResultCode resultCode,
    SystemError::ErrorCode sysErrorCode,
    std::unique_ptr<AbstractOutgoingTunnelConnection> connection)
{
    NX_LOGX(lm("cross-nat %1. Connector has finished with result: %2, %3")
        .arg(m_connectSessionId).arg(QnLexical::serialized(resultCode))
        .arg(SystemError::toString(sysErrorCode)),
        cl_logDEBUG2);

    auto connector = std::move(*connectorIter);
    m_connectors.erase(connectorIter);

    if (resultCode != api::NatTraversalResultCode::ok && !m_connectors.empty())
        return;     // Waiting for other connectors to complete.

    NX_CRITICAL((resultCode != api::NatTraversalResultCode::ok) || connection);
    m_connectors.clear();   // Cancelling other connectors.

    nx::utils::swapAndCall(m_handler, resultCode, sysErrorCode, std::move(connection));
}

} // namespace cloud
} // namespace network
} // namespace nx
