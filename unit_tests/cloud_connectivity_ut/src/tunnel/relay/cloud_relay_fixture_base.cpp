#include "cloud_relay_fixture_base.h"

#include "client_to_relay_connection.h"

namespace nx {
namespace network {
namespace cloud {
namespace relay {
namespace test {

CloudRelayFixtureBase::CloudRelayFixtureBase()
{
    using namespace std::placeholders;

    m_clientToRelayConnectionFactoryBak =
        api::ClientToRelayConnectionFactory::setCustomFactoryFunc(
            std::bind(&CloudRelayFixtureBase::clientToRelayConnectionFactoryFunc, this, _1));
}

CloudRelayFixtureBase::~CloudRelayFixtureBase()
{
    api::ClientToRelayConnectionFactory::setCustomFactoryFunc(
        std::move(m_clientToRelayConnectionFactoryBak));
}

std::unique_ptr<api::ClientToRelayConnection> 
    CloudRelayFixtureBase::clientToRelayConnectionFactoryFunc(
        const SocketAddress& /*relayEndpoint*/)
{
    auto result = std::make_unique<ClientToRelayConnection>();
    result->setOnBeforeDestruction(
        std::bind(&CloudRelayFixtureBase::onClientToRelayConnectionDestroyed, this, result.get()));
    onClientToRelayConnectionInstanciated(result.get());
    return std::move(result);
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
