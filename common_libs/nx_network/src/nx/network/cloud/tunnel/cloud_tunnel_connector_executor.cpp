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
    m_connectors = ConnectorFactory::instance().create(
        targetAddress,
        connectSessionId,
        response,
        std::move(udpSocket));

    bindToAioThread(getAioThread());
}

void ConnectorExecutor::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    for (auto& connector : m_connectors)
        connector->bindToAioThread(aioThread);
}

void ConnectorExecutor::setTimeout(std::chrono::milliseconds timeout)
{
    m_connectTimeout = timeout;
}

void ConnectorExecutor::start(CompletionHandler handler)
{
    using namespace std::placeholders;

    m_handler = std::move(handler);

    if (m_connectors.empty())
    {
        post(
            [this]()
            {
                nx::utils::swapAndCall(
                    m_handler,
                    nx::hpm::api::NatTraversalResultCode::noSuitableTraversalMethod,
                    SystemError::hostUnreach,
                    nullptr);
            });
        return;
    }

    NX_ASSERT(!m_connectors.empty());
    // TODO: #ak sorting connectors by priority

    NX_LOGX(lm("cross-nat %1. Starting %2 connectors")
        .arg(m_connectSessionId).arg(m_connectors.size()),
        cl_logDEBUG2);

    for (auto it = m_connectors.begin(); it != m_connectors.end(); ++it)
    {
        (*it)->bindToAioThread(getAioThread());
        (*it)->connect(
            m_response,
            m_connectTimeout,
            std::bind(&ConnectorExecutor::onConnectorFinished, this,
                it, _1, _2, _3));
    }
}

void ConnectorExecutor::stopWhileInAioThread()
{
    m_connectors.clear();
}

void ConnectorExecutor::onConnectorFinished(
    CloudConnectors::iterator connectorIter,
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
