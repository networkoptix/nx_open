#include "transport_manager.h"

namespace nx::data_sync_engine {

std::unique_ptr<AbstractTransactionTransportConnector> TransportManager::createConnector(
    const std::string& /*systemId*/,
    const std::string& /*connectionId*/,
    const nx::utils::Url& /*targetUrl*/)
{
    // TODO
    return nullptr;
}

} // namespace nx::data_sync_engine
