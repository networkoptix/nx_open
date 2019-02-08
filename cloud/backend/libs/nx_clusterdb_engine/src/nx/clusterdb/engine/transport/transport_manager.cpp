#include "transport_manager.h"

#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/url/url_builder.h>

#include "connector_factory.h"
#include "../http/http_paths.h"

namespace nx::clusterdb::engine::transport {

TransportManager::TransportManager(
    const ProtocolVersionRange& protocolVersionRange,
    CommandLog* transactionLog,
    const OutgoingCommandFilter& outgoingCommandFilter,
    const std::string& nodeId)
    :
    m_protocolVersionRange(protocolVersionRange),
    m_commandLog(transactionLog),
    m_outgoingCommandFilter(outgoingCommandFilter),
    m_nodeId(nodeId)
{
}

std::unique_ptr<AbstractTransactionTransportConnector> TransportManager::createConnector(
    const std::string& systemId,
    const std::string& /*connectionId*/,
    const nx::utils::Url& targetUrl)
{
    using namespace nx::network;

    const auto syncApiTargetUrl = url::Builder(targetUrl).appendPath(
        http::rest::substituteParameters(kBaseSynchronizationPath, {systemId})).toUrl();

    // TODO: #ak Is this class really needed?

    return ConnectorFactory::instance().create(
        m_protocolVersionRange,
        m_commandLog,
        m_outgoingCommandFilter,
        syncApiTargetUrl,
        systemId,
        m_nodeId);
}

} // namespace nx::clusterdb::engine::transport
