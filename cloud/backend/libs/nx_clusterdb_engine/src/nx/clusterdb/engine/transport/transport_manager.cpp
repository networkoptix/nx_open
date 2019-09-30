#include "transport_manager.h"

#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/url/url_builder.h>

#include "http_tunnel/factory.h"
#include "../http/http_paths.h"

namespace nx::clusterdb::engine::transport {

TransportManager::TransportManager(
    const ProtocolVersionRange& protocolVersionRange,
    CommandLog* transactionLog,
    const OutgoingCommandFilter& outgoingCommandFilter,
    ConnectionManager* connectionManager,
    const std::string& nodeId)
    :
    m_protocolVersionRange(protocolVersionRange),
    m_commandLog(transactionLog),
    m_outgoingCommandFilter(outgoingCommandFilter),
    m_connectionManager(connectionManager),
    m_nodeId(nodeId)
{
    registerTransport(std::make_unique<http_tunnel::Factory>());
}

void TransportManager::registerTransport(std::unique_ptr<AbstractFactory> transportFactory)
{
    m_transportFactories.push_back(std::move(transportFactory));
}

std::vector<std::unique_ptr<AbstractAcceptor>> TransportManager::createAllAcceptors(
    const QnUuid& nodeId)
{
    std::vector<std::unique_ptr<AbstractAcceptor>> result;
    std::transform(
        m_transportFactories.begin(), m_transportFactories.end(),
        std::back_inserter(result),
        [this, nodeId](auto& factory)
        {
            return factory->createAcceptor(
                m_protocolVersionRange,
                m_commandLog,
                m_outgoingCommandFilter,
                m_connectionManager,
                nodeId);
        });
    return result;
}

bool TransportManager::setConnectorTypeKey(const std::string& key)
{
    auto it = std::find_if(
        m_transportFactories.begin(), m_transportFactories.end(),
        [key](auto& factory) { return factory->key() == key; });

    if (it == m_transportFactories.end())
        return false;

    m_connectorFactory = it->get();
    return true;
}

std::unique_ptr<AbstractTransactionTransportConnector> TransportManager::createConnector(
    const std::string& clusterId,
    const std::string& /*connectionId*/,
    const nx::utils::Url& targetUrl)
{
    auto connectorFactory = m_connectorFactory
        ? m_connectorFactory
        : m_transportFactories.front().get();

    return connectorFactory->createConnector(
        m_protocolVersionRange,
        m_commandLog,
        m_outgoingCommandFilter,
        clusterId,
        m_nodeId,
        targetUrl);
}

} // namespace nx::clusterdb::engine::transport
