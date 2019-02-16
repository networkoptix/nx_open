#include <nx/clusterdb/engine/transport/connector_factory.h>
#include <nx/clusterdb/engine/transport/p2p_http/connector.h>

#include "connection_acceptance_tests.h"
#include "../node_synchronization_tests.h"

namespace nx::clusterdb::engine::transport::test {

class P2pHttpTransportInstaller
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
                return std::make_unique<p2p::http::Connector>(
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
    P2pHttp,
    ConnectionAcceptance,
    P2pHttpTransportInstaller);

//-------------------------------------------------------------------------------------------------

//using namespace engine::test;
//
//INSTANTIATE_TYPED_TEST_CASE_P(
//    P2pHttp,
//    Synchronization,
//    P2pHttpTransportInstaller);

} // namespace nx::clusterdb::engine::transport::test
