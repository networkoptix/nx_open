#include <nx/clusterdb/engine/transport/common_http/acceptor.h>
#include <nx/clusterdb/engine/transport/common_http/connector.h>
#include <nx/clusterdb/engine/transport/common_http/factory.h>

#include "connection_acceptance_tests.h"
#include "transport_acceptance_tests.h"
#include "../node_synchronization_tests.h"

namespace nx::clusterdb::engine::transport::test {

struct HttpTransportTypes
{
    using Acceptor = CommonHttpAcceptor;
    using CommandPipeline = CommonHttpConnection;
    using Connector = HttpCommandPipelineConnector;
};

INSTANTIATE_TYPED_TEST_CASE_P(
    Http,
    CommandPipelineAcceptance,
    HttpTransportTypes);

//-------------------------------------------------------------------------------------------------

class HttpTransportInstaller
{
public:
    static constexpr auto kConnectorTypeKey = common_http::Factory::kKey;
};

INSTANTIATE_TYPED_TEST_CASE_P(
    Http,
    ConnectionAcceptance,
    HttpTransportInstaller);

//-------------------------------------------------------------------------------------------------

using namespace engine::test;

INSTANTIATE_TYPED_TEST_CASE_P(
    Http,
    Synchronization,
    HttpTransportInstaller);

} // namespace nx::clusterdb::engine::transport::test
