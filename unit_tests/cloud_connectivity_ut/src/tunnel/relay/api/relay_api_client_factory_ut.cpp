#include <functional>
#include <map>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/api/relay_api_client_factory.h>

#include "relay_api_client_stub.h"

namespace nx::cloud::relay::api::test {

namespace {

class AbstractClientTypeHolder
{
public:
    virtual ~AbstractClientTypeHolder() = default;

    virtual void registerType(api::ClientFactory* factory, int priority) const = 0;
    virtual bool isSameType(api::Client* client) const = 0;
};

template<typename T>
class ClientTypeHolder:
    public AbstractClientTypeHolder
{
public:
    virtual void registerType(
        api::ClientFactory* factory,
        int priority) const override
    {
        factory->registerClientType<T>(priority);
    }

    virtual bool isSameType(api::Client* client) const override
    {
        return dynamic_cast<T*>(client) != nullptr;
    }
};

template<int id>
class MarkedClientStub:
    public ClientStub
{
public:
    MarkedClientStub(const nx::utils::Url& url):
        ClientStub(url)
    {
    }
};

} // namespace

class RelayApiClientFactory:
    public ::testing::Test
{
public:
    RelayApiClientFactory():
        m_relayUrl("http://127.0.0.1/relay/")
    {
    }

protected:
    void givenMultipleClientTypesWithDifferentPriorities()
    {
        constexpr int kBasePriority = 1000;

        registerClient<kBasePriority + 1>();
        registerClient<kBasePriority + 2>();
        registerClient<kBasePriority + 3>();
    }

    void whenInstantiateClient()
    {
        m_client = m_factory.create(m_relayUrl);
    }

    void thenClientWithTheHighestPriorityIsSelected()
    {
        ASSERT_TRUE(m_clientTypesByPriority.begin()->second->isSameType(m_client.get()));
    }

private:
    const nx::utils::Url m_relayUrl;
    api::ClientFactory m_factory;
    std::unique_ptr<api::Client> m_client;
    std::map<
        int, std::unique_ptr<AbstractClientTypeHolder>, std::greater<int>
    > m_clientTypesByPriority;

    template<int ClientTypeId>
    void registerClient()
    {
        auto typeHolder =
            std::make_unique<ClientTypeHolder<MarkedClientStub<ClientTypeId>>>();
        typeHolder->registerType(&m_factory, ClientTypeId);
        m_clientTypesByPriority.emplace(ClientTypeId, std::move(typeHolder));
    }
};

TEST_F(RelayApiClientFactory, type_with_highest_priority_is_selected)
{
    givenMultipleClientTypesWithDifferentPriorities();
    whenInstantiateClient();
    thenClientWithTheHighestPriorityIsSelected();
}

// TEST_F(RelayApiClientFactory, type_priority_is_decreased_on_feedback)

} // namespace nx::cloud::relay::api::test
