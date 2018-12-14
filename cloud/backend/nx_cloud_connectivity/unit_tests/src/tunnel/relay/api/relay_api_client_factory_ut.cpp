#include <functional>
#include <map>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/api/relay_api_client_factory.h>
#include <nx/utils/random.h>
#include <nx/utils/type_utils.h>

#include "relay_api_client_stub.h"

namespace nx::cloud::relay::api::test {

namespace {

class AbstractClientTypeHolder
{
public:
    virtual ~AbstractClientTypeHolder() = default;

    virtual void registerType(api::ClientFactory* factory) const = 0;
    virtual bool isSameType(api::Client* client) const = 0;
};

template<typename T>
class ClientTypeHolder:
    public AbstractClientTypeHolder
{
public:
    virtual void registerType(api::ClientFactory* factory) const override
    {
        factory->registerClientType<T>();
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
    virtual void SetUp() override
    {
        givenMultipleClientTypesWithDifferentPriorities();
    }

    void givenMultipleClientTypesWithDifferentPriorities()
    {
        constexpr int kBasePriority = 1000;

        registerClient<kBasePriority + 1>();
        registerClient<kBasePriority + 2>();
        registerClient<kBasePriority + 3>();
    }

    void givenSelectedOperationalClientType()
    {
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
            nx::utils::random::number<int>(0, (int)m_clientTypes.size() - 1);
    }

    void thenClientWithTheHighestPriorityIsSelected()
    {
        ASSERT_TRUE(m_clientTypes.front()->isSameType(m_client.get()));
    }

    void thenClientWithTheNextPriorityIsSelected()
    {
        whenInstantiateClient();

        auto client = static_cast<BasicMarkedClientStub*>(m_client.get());
        ASSERT_TRUE((*std::next(m_clientTypes.begin()))->isSameType(client));
    }

    void thenOperationalClientTypeIsSelectedEventually()
    {
        for (;;)
        {
            whenInstantiateClient();

            if (m_clientTypes[m_operationalClientTypeId]->isSameType(m_client.get()))
                break;
            static_cast<BasicMarkedClientStub*>(m_client.get())->reportFailureToFactory();
        }
    }

    void thenOperationalClientTypeIsUsed()
    {
        for (int i = 0; i < 5; ++i)
        {
            whenInstantiateClient();

            ASSERT_TRUE(m_clientTypes[m_operationalClientTypeId]
                ->isSameType(m_client.get()));
        }
    }

    std::tuple<
        std::unique_ptr<BasicMarkedClientStub>,
        std::unique_ptr<BasicMarkedClientStub>
    > createTwoClients()
    {
        auto one = m_factory.create(m_relayUrl);
        auto two = m_factory.create(m_relayUrl);
        return std::make_tuple(
            nx::utils::static_unique_ptr_cast<BasicMarkedClientStub>(std::move(one)),
            nx::utils::static_unique_ptr_cast<BasicMarkedClientStub>(std::move(two)));
    }

    void whenClientGivesNegativeFeedback(
        const std::unique_ptr<BasicMarkedClientStub>& client)
    {
        client->reportFailureToFactory();
    }

    void rememberSelectedClientType()
    {
        auto client = m_factory.create(m_relayUrl);
        auto clientTypeIter = std::find_if(
            m_clientTypes.begin(),
            m_clientTypes.end(),
            [&client](const auto& typeHolder) { return typeHolder->isSameType(client.get()); });
        m_remeberedClientTypeIndex = clientTypeIter - m_clientTypes.begin();
    }

    void assertSelectedClientTypeWasNotChanged()
    {
        auto client = m_factory.create(m_relayUrl);
        ASSERT_TRUE(m_clientTypes[m_remeberedClientTypeIndex]->isSameType(client.get()));
    }

private:
    const nx::utils::Url m_relayUrl;
    api::ClientFactory m_factory;
    std::unique_ptr<api::Client> m_client;
    std::deque<std::unique_ptr<AbstractClientTypeHolder>> m_clientTypes;
    int m_operationalClientTypeId = -1;
    int m_remeberedClientTypeIndex = -1;

    template<int ClientTypeId>
    void registerClient()
    {
        auto typeHolder =
            std::make_unique<ClientTypeHolder<MarkedClientStub<ClientTypeId>>>();
        typeHolder->registerType(&m_factory);
        m_clientTypes.emplace_front(std::move(typeHolder));
    }
};

TEST_F(RelayApiClientFactory, type_with_highest_priority_is_selected)
{
    whenInstantiateClient();
    thenClientWithTheHighestPriorityIsSelected();
}

TEST_F(RelayApiClientFactory, type_priority_is_decreased_on_feedback)
{
    whenClientWithHighestPriorityReportsMultipleFails();
    thenClientWithTheNextPriorityIsSelected();
}

TEST_F(RelayApiClientFactory, operational_client_type_is_selected_eventually)
{
    whenOnlyOneRandomClientTypeIsOperational();
    thenOperationalClientTypeIsSelectedEventually();
}

TEST_F(RelayApiClientFactory, operational_client_type_is_used_when_selected)
{
    givenSelectedOperationalClientType();
    thenOperationalClientTypeIsUsed();
}

TEST_F(RelayApiClientFactory, not_selected_client_feedback_does_not_change_anything)
{
    auto [one, two] = createTwoClients();

    whenClientGivesNegativeFeedback(one);
    rememberSelectedClientType();
    whenClientGivesNegativeFeedback(two);

    assertSelectedClientTypeWasNotChanged();
}

} // namespace nx::cloud::relay::api::test
