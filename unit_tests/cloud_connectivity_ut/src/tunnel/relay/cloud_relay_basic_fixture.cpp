#include "cloud_relay_basic_fixture.h"

#include "client_to_relay_connection.h"

namespace nx {
namespace network {
namespace cloud {
namespace relay {
namespace test {

using namespace nx::cloud::relay;

BasicFixture::BasicFixture()
{
    using namespace std::placeholders;

    m_clientFactoryBak =
        api::ClientFactory::setCustomFactoryFunc(
            std::bind(&BasicFixture::clientFactoryFunc, this, _1));
}

BasicFixture::~BasicFixture()
{
    api::ClientFactory::setCustomFactoryFunc(
        std::move(m_clientFactoryBak));
}

std::unique_ptr<api::Client> 
    BasicFixture::clientFactoryFunc(const QUrl& /*relayUrl*/)
{
    auto result = std::make_unique<ClientToRelayConnection>();
    result->setOnBeforeDestruction(
        std::bind(&BasicFixture::onClientToRelayConnectionDestroyed, this,
            result.get()));
    onClientToRelayConnectionInstanciated(result.get());
    return std::move(result);
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
