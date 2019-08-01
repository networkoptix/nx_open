#include <nx/clusterdb/engine/transport/p2p_websocket/connector.h>
#include <nx/clusterdb/engine/transport/p2p_websocket/factory.h>

#include "connection_acceptance_tests.h"
#include "../node_synchronization_tests.h"

namespace nx::clusterdb::engine::transport::test {

class P2pWebsocketTransportInstaller
{
public:
    static constexpr auto kConnectorTypeKey = p2p::websocket::Factory::kKey;
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
