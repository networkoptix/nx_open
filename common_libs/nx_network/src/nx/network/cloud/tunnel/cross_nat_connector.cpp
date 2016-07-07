/**********************************************************
* Jul 7, 2016
* akolesnikov
***********************************************************/

#include "cross_nat_connector.h"

#include "connector_factory.h"


namespace nx {
namespace network {
namespace cloud {

CrossNatConnector::CrossNatConnector(const AddressEntry& targetPeerAddress)
:
    m_targetPeerAddress(targetPeerAddress)
{
}

CrossNatConnector::~CrossNatConnector()
{
    stopWhileInAioThread();
}

void CrossNatConnector::connect(
    std::chrono::milliseconds timeout,
    nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractOutgoingTunnelConnection>)> handler)
{
    post(
        [this, timeout, handler = std::move(handler)]() mutable
        {
            m_connectors = ConnectorFactory::createAllCloudConnectors(m_targetPeerAddress);
            m_completionHandler = std::move(handler);
            for (auto& connector : m_connectors)
            {
                auto connectorType = connector.first;
                connector.second->bindToAioThread(getAioThread());
                connector.second->connect(
                    timeout,
                    [connectorType, this](
                        SystemError::ErrorCode errorCode,
                        std::unique_ptr<AbstractOutgoingTunnelConnection> connection)
                    {
                        onConnectorFinished(
                            connectorType,
                            errorCode,
                            std::move(connection));
                    });
            }
        });
}

void CrossNatConnector::stopWhileInAioThread()
{
    m_connectors.clear();
}

void CrossNatConnector::onConnectorFinished(
    CloudConnectType connectorType,
    SystemError::ErrorCode errorCode,
    std::unique_ptr<AbstractOutgoingTunnelConnection> connection)
{
    const auto connectorIter = m_connectors.find(connectorType);
    if (connectorIter == m_connectors.end())
        return; //it can happen when stopping OutgoingTunnel
    auto connector = std::move(connectorIter->second);
    m_connectors.erase(connectorIter);

    if (errorCode != SystemError::noError && !m_connectors.empty())
        return;     //waiting for other connectors to complete

    NX_CRITICAL((errorCode != SystemError::noError) || connection);
    m_connectors.clear();   //cancelling other connectors
    auto completionHandler = std::move(m_completionHandler);
    m_completionHandler = nullptr;
    completionHandler(errorCode, std::move(connection));
}

}   //namespace cloud
}   //namespace network
}   //namespace nx
