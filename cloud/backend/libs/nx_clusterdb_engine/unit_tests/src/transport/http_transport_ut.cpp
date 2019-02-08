#include <nx/clusterdb/engine/transport/connector_factory.h>
#include <nx/clusterdb/engine/transport/common_http/acceptor.h>
#include <nx/clusterdb/engine/transport/common_http/connector.h>

#include "connection_acceptance_tests.h"
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
    CommandPipelineAcceptance,
    HttpTransportTypes);

//-------------------------------------------------------------------------------------------------

class HttpTransportInstaller
{
public:
    static ConnectorFactory::Function configureFactory(ConnectorFactory* factory)
    {
        return factory->setCustomFunc(
            [](const ProtocolVersionRange& protocolVersionRange,
                CommandLog* commandLog,
                const OutgoingCommandFilter& outgoingCommandFilter,
                const nx::utils::Url& nodeUrl,
                const std::string& systemId,
                const std::string& nodeId)
            {
                return std::make_unique<HttpTransportConnector>(
                    protocolVersionRange,
                    commandLog,
                    outgoingCommandFilter,
                    nodeUrl,
                    systemId,
                    nodeId);
            });
    }
};

INSTANTIATE_TYPED_TEST_CASE_P(
    Http,
    ConnectionAcceptance,
    HttpTransportInstaller);

} // namespace nx::clusterdb::engine::transport::test
