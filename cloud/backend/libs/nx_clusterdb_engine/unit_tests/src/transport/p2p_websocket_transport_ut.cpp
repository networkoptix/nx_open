#include <nx/clusterdb/engine/transport/connector_factory.h>
#include <nx/clusterdb/engine/transport/p2p_websocket/connector.h>

#include "connection_acceptance_tests.h"
#include "../node_synchronization_tests.h"

namespace nx::clusterdb::engine::transport::test {

class P2pWebsocketTransportInstaller
{
public:
    static ConnectorFactory::Function configureFactory(ConnectorFactory* factory)
    {
        return factory->setCustomFunc(
            [](const ProtocolVersionRange& protocolVersionRange,
                CommandLog* commandLog,
                const OutgoingCommandFilter& outgoingCommandFilter,
                const std::string& clusterId,
                const std::string& nodeId,
                const nx::utils::Url& nodeUrl)
            {
                return std::make_unique<p2p::websocket::Connector>(
                    protocolVersionRange,
                    commandLog,
                    outgoingCommandFilter,
                    clusterId,
                    nodeId,
                    nodeUrl);
            });
    }
};

INSTANTIATE_TYPED_TEST_CASE_P(
    P2pWebsocket,
    ConnectionAcceptance,
    P2pWebsocketTransportInstaller);

//-------------------------------------------------------------------------------------------------

using namespace engine::test;

INSTANTIATE_TYPED_TEST_CASE_P(
    P2pWebsocket,
    Synchronization,
    P2pWebsocketTransportInstaller);

} // namespace nx::clusterdb::engine::transport::test
