#pragma once

#include <memory>
#include <string>

#include <nx/utils/url.h>

#include "abstract_transaction_transport_connector.h"

namespace nx::clusterdb::engine::transport {

class TransportManager
{
public:
    std::unique_ptr<AbstractTransactionTransportConnector> createConnector(
        const std::string& systemId,
        const std::string& connectionId,
        const nx::utils::Url& targetUrl);
};

} // namespace nx::clusterdb::engine::transport
