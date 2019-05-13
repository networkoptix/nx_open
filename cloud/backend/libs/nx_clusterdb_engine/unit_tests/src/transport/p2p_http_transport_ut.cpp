#include <nx/clusterdb/engine/transport/p2p_http/connector.h>
#include <nx/clusterdb/engine/transport/p2p_http/factory.h>

#include "connection_acceptance_tests.h"
#include "../node_synchronization_tests.h"

namespace nx::clusterdb::engine::transport::test {

class P2pHttpTransportInstaller
{
public:
    static constexpr auto kConnectorTypeKey = p2p::http::Factory::kKey;
};

INSTANTIATE_TYPED_TEST_CASE_P(
    P2pHttp,
    ConnectionAcceptance,
    P2pHttpTransportInstaller);

//-------------------------------------------------------------------------------------------------

using namespace engine::test;

INSTANTIATE_TYPED_TEST_CASE_P(
    P2pHttp,
    Synchronization,
    P2pHttpTransportInstaller);

} // namespace nx::clusterdb::engine::transport::test
