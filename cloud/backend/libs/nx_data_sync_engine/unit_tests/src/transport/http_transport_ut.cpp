#include <nx/data_sync_engine/transport/http_transport_acceptor.h>
#include <nx/data_sync_engine/transport/http_transport_connector.h>

#include "transport_acceptance_tests.h"

namespace nx::clusterdb::engine::transport::test {

struct HttpTransportTypes
{
    using Acceptor = CommonHttpAcceptor;
    using CommandPipeline = CommonHttpConnection;
    using Connector = HttpCommandPipelineConnector;
};

INSTANTIATE_TYPED_TEST_CASE_P(
    Http,
    TransportAcceptance,
    HttpTransportTypes);

} // namespace nx::clusterdb::engine::transport::test
