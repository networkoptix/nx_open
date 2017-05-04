#include "cloud_relay_fixture_base.h"

#include "client_to_relay_connection.h"

namespace nx {
namespace network {
namespace cloud {
namespace relay {
namespace test {

using namespace nx::cloud::relay;

CloudRelayFixtureBase::CloudRelayFixtureBase()
{
    using namespace std::placeholders;

    m_clientFactoryBak =
        api::ClientFactory::setCustomFactoryFunc(
            std::bind(&CloudRelayFixtureBase::clientFactoryFunc, this, _1));
}

CloudRelayFixtureBase::~CloudRelayFixtureBase()
{
    api::ClientFactory::setCustomFactoryFunc(
        std::move(m_clientFactoryBak));
}

std::unique_ptr<api::Client> 
    CloudRelayFixtureBase::clientFactoryFunc(const QUrl& /*relayUrl*/)
{
    auto result = std::make_unique<ClientToRelayConnection>();
    result->setOnBeforeDestruction(
        std::bind(&CloudRelayFixtureBase::onClientToRelayConnectionDestroyed, this,
            result.get()));
    onClientToRelayConnectionInstanciated(result.get());
    return std::move(result);
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
