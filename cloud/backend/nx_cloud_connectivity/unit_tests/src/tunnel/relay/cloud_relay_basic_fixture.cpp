#include "cloud_relay_basic_fixture.h"

#include "api/relay_api_client_stub.h"

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
        api::detail::ClientFactory::instance().setCustomFunc(
            std::bind(&BasicFixture::clientFactoryFunc, this, _1));
}

BasicFixture::~BasicFixture()
{
    resetClientFactoryToDefault();
}

void BasicFixture::resetClientFactoryToDefault()
{
    if (m_clientFactoryBak)
    {
        api::detail::ClientFactory::instance().setCustomFunc(std::move(*m_clientFactoryBak));
        m_clientFactoryBak = boost::none;
    }
}

std::unique_ptr<api::AbstractClient>
    BasicFixture::clientFactoryFunc(const nx::utils::Url& /*relayUrl*/)
{
    auto result = std::make_unique<nx::cloud::relay::api::test::ClientStub>();
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
