#include <functional>
#include <map>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/api/relay_api_client_factory.h>
#include <nx/utils/random.h>

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

class BasicMarkedClientStub:
    public ClientStub
{
    using base_type = ClientStub;

public:
    BasicMarkedClientStub(
        const nx::utils::Url& url,
        ClientFeedbackFunction feedbackFunction)
        :
        base_type(url),
        m_feedbackFunction(std::move(feedbackFunction))
    {
    }

    virtual int typeId() const = 0;

    void reportFailureToFactory()
    {
        m_feedbackFunction(api::ResultCode::networkError);
    }

private:
    ClientFeedbackFunction m_feedbackFunction;
};

template<int id>
class MarkedClientStub:
    public BasicMarkedClientStub
{
    using base_type = BasicMarkedClientStub;

public:
    MarkedClientStub(
        const nx::utils::Url& url,
        ClientFeedbackFunction feedbackFunction)
        :
        base_type(url, std::move(feedbackFunction))
    {
    }

    virtual int typeId() const override
    {
        return id;
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

    void givenSelectedOperationalClientType()
    {
        givenMultipleClientTypesWithDifferentPriorities();
        whenOnlyOneRandomClientTypeIsOperational();
        thenOperationalClientTypeIsSelectedEventually();
    }

    void whenInstantiateClient()
    {
        m_client = m_factory.create(m_relayUrl);
    }

    void whenClientWithHighestPriorityReportsMultipleFails()
    {
        std::optional<int> clientTypeId;
        for (int i = 0; i < 99; ++i)
        {
            whenInstantiateClient();

            auto client = static_cast<BasicMarkedClientStub*>(m_client.get());
            if (!clientTypeId)
                clientTypeId = client->typeId();
            else if (*clientTypeId != client->typeId())
                break;

            client->reportFailureToFactory();
        }
    }

    void whenOnlyOneRandomClientTypeIsOperational()
    {
        m_operationalClientTypeId =
            nx::utils::random::number<int>(0, (int) m_clientTypesByPriority.size()-1);
    }

    void thenClientWithTheHighestPriorityIsSelected()
    {
        ASSERT_TRUE(m_clientTypesByPriority.front()->isSameType(m_client.get()));
    }

    void thenClientWithTheNextPriorityIsSelected()
    {
        whenInstantiateClient();

        auto client = static_cast<BasicMarkedClientStub*>(m_client.get());
        ASSERT_TRUE((*std::next(m_clientTypesByPriority.begin()))->isSameType(client));
    }

    void thenOperationalClientTypeIsSelectedEventually()
    {
        for (;;)
        {
            whenInstantiateClient();

            if (m_clientTypesByPriority[m_operationalClientTypeId]->isSameType(m_client.get()))
                break;
            static_cast<BasicMarkedClientStub*>(m_client.get())->reportFailureToFactory();
        }
    }

    void thenOperationalClientTypeIsUsed()
    {
        for (int i = 0; i < 5; ++i)
        {
            whenInstantiateClient();

            ASSERT_TRUE(m_clientTypesByPriority[m_operationalClientTypeId]
                ->isSameType(m_client.get()));
        }
    }

private:
    const nx::utils::Url m_relayUrl;
    api::ClientFactory m_factory;
    std::unique_ptr<api::Client> m_client;
    std::deque<std::unique_ptr<AbstractClientTypeHolder>> m_clientTypesByPriority;
    int m_operationalClientTypeId = -1;

    template<int ClientTypeId>
    void registerClient()
    {
        auto typeHolder =
            std::make_unique<ClientTypeHolder<MarkedClientStub<ClientTypeId>>>();
        typeHolder->registerType(&m_factory, ClientTypeId);
        m_clientTypesByPriority.emplace_front(std::move(typeHolder));
    }
};

TEST_F(RelayApiClientFactory, type_with_highest_priority_is_selected)
{
    givenMultipleClientTypesWithDifferentPriorities();
    whenInstantiateClient();
    thenClientWithTheHighestPriorityIsSelected();
}

TEST_F(RelayApiClientFactory, type_priority_is_decreased_on_feedback)
{
    givenMultipleClientTypesWithDifferentPriorities();
    whenClientWithHighestPriorityReportsMultipleFails();
    thenClientWithTheNextPriorityIsSelected();
}

TEST_F(RelayApiClientFactory, operational_client_type_is_selected_eventually)
{
    givenMultipleClientTypesWithDifferentPriorities();
    whenOnlyOneRandomClientTypeIsOperational();
    thenOperationalClientTypeIsSelectedEventually();
}

TEST_F(RelayApiClientFactory, operational_client_type_is_used_when_selected)
{
    givenSelectedOperationalClientType();
    thenOperationalClientTypeIsUsed();
}

} // namespace nx::cloud::relay::api::test
