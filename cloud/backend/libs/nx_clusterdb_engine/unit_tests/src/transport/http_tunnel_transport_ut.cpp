#include <nx/clusterdb/engine/transport/http_tunnel/acceptor.h>
#include <nx/clusterdb/engine/transport/http_tunnel/connector.h>
#include <nx/clusterdb/engine/transport/http_tunnel/factory.h>

#include "connection_acceptance_tests.h"
#include "transport_acceptance_tests.h"
#include "../node_synchronization_tests.h"

namespace nx::clusterdb::engine::transport::test {

class HttpTunnelTransportInstaller
{
public:
    static constexpr auto kConnectorTypeKey = http_tunnel::Factory::kKey;
};

INSTANTIATE_TYPED_TEST_CASE_P(
    HttpTunnel,
    ConnectionAcceptance,
    HttpTunnelTransportInstaller);

//-------------------------------------------------------------------------------------------------

using namespace engine::test;

INSTANTIATE_TYPED_TEST_CASE_P(
    HttpTunnel,
    Synchronization,
    HttpTunnelTransportInstaller);

} // namespace nx::clusterdb::engine::transport::test
