#include <memory>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/api/relay_api_client.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/http/fusion_data_http_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/random.h>

#include <nx/cloud/relay/controller/connect_session_manager.h>
#include <nx/cloud/relay/statistics_provider.h>
#include <nx/cloud/relaying/listening_peer_manager.h>

#include "connect_session_manager_mock.h"
#include "listening_peer_manager_mock.h"
#include "../basic_component_test.h"

namespace nx {
namespace cloud {
namespace relay {
namespace test {

class HttpApi:
    public ::testing::Test,
    public BasicComponentTest
{
protected:
    utils::SyncQueue<api::BeginListeningRequest> m_receivedBeginListeningRequests;
    utils::SyncQueue<api::CreateClientSessionRequest> m_receivedCreateClientSessionRequests;
    utils::SyncQueue<api::ConnectToPeerRequest> m_receivedConnectToPeerRequests;

    void thenRequestSucceeded()
    {
        ASSERT_EQ(api::ResultCode::ok, m_apiResponse.pop());
    }

    api::Client& relayClient()
    {
        if (!m_relayClient)
            m_relayClient = api::ClientFactory::create(basicUrl());
        return *m_relayClient;
    }

    void onRequestCompletion(api::ResultCode resultCode)
    {
        m_apiResponse.push(resultCode);
    }

    nx::utils::Url basicUrl() const
    {
        return nx::network::url::Builder().setScheme("http").setHost("127.0.0.1")
            .setPort(moduleInstance()->httpEndpoints()[0].port).toUrl();
    }

private:
    controller::ConnectSessionManagerFactory::FactoryFunc m_connectionSessionManagerFactoryFuncBak;
    relaying::ListeningPeerManagerFactory::Function m_listeningPeerManagerFactoryFuncBak;
    ConnectSessionManagerMock* m_connectSessionManager = nullptr;
    ListeningPeerManagerMock* m_listeningPeerManager = nullptr;
    nx::utils::SyncQueue<api::ResultCode> m_apiResponse;
    std::unique_ptr<api::Client> m_relayClient;

    virtual void SetUp() override
    {
        using namespace std::placeholders;

        m_connectionSessionManagerFactoryFuncBak =
            controller::ConnectSessionManagerFactory::setFactoryFunc(
                std::bind(&HttpApi::createConnectSessionManager, this,
                    _1, _2, _3, _4));

        m_listeningPeerManagerFactoryFuncBak =
            relaying::ListeningPeerManagerFactory::instance().setCustomFunc(
                std::bind(&HttpApi::createListeningPeerManager, this,
                    _1, _2));

        ASSERT_TRUE(startAndWaitUntilStarted());
    }

    virtual void TearDown() override
    {
        if (m_relayClient)
            m_relayClient->pleaseStopSync();
        stop();

        relaying::ListeningPeerManagerFactory::instance().setCustomFunc(
            std::move(m_listeningPeerManagerFactoryFuncBak));

        controller::ConnectSessionManagerFactory::setFactoryFunc(
            std::move(m_connectionSessionManagerFactoryFuncBak));
    }

    std::unique_ptr<controller::AbstractConnectSessionManager>
        createConnectSessionManager(
            const conf::Settings& /*settings*/,
            model::ClientSessionPool* /*clientSessionPool*/,
            relaying::ListeningPeerPool* /*listeningPeerPool*/,
            controller::AbstractTrafficRelay* /*trafficRelay*/)
    {
        auto connectSessionManager =
            std::make_unique<ConnectSessionManagerMock>(
                &m_receivedCreateClientSessionRequests,
                &m_receivedConnectToPeerRequests);
        m_connectSessionManager = connectSessionManager.get();
        return std::move(connectSessionManager);
    }

    std::unique_ptr<relaying::AbstractListeningPeerManager>
        createListeningPeerManager(
            const relaying::Settings& /*settings*/,
            relaying::ListeningPeerPool* /*listeningPeerPool*/)
    {
        auto listeningPeerManager =
            std::make_unique<ListeningPeerManagerMock>(
                &m_receivedBeginListeningRequests);
        m_listeningPeerManager = listeningPeerManager.get();
        return std::move(listeningPeerManager);
    }
};

//-------------------------------------------------------------------------------------------------
// HttpApiBeginListening

class HttpApiBeginListening:
    public HttpApi
{
protected:
    void whenIssuedApiRequest()
    {
        using namespace std::placeholders;

        relayClient().beginListening(
            "some_server_name",
            std::bind(&HttpApiBeginListening::onRequestCompletion, this, _1));
    }

    void thenRequestHasBeenDeliveredToTheManager()
    {
        m_receivedBeginListeningRequests.pop();
    }
};

TEST_F(HttpApiBeginListening, request_is_delivered)
{
    whenIssuedApiRequest();
    thenRequestSucceeded();
    thenRequestHasBeenDeliveredToTheManager();
}

//-------------------------------------------------------------------------------------------------
// HttpApiCreateClientSession

class HttpApiCreateClientSession:
    public HttpApi
{
protected:
    void whenIssuedApiRequest()
    {
        using namespace std::placeholders;

        relayClient().startSession(
            "some_session_id",
            "some_server_name",
            std::bind(&HttpApiCreateClientSession::onRequestCompletion, this, _1));
    }

    void thenRequestHasBeenDeliveredToTheManager()
    {
        m_receivedCreateClientSessionRequests.pop();
    }
};

TEST_F(HttpApiCreateClientSession, request_is_delivered)
{
    whenIssuedApiRequest();
    thenRequestSucceeded();
    thenRequestHasBeenDeliveredToTheManager();
}

//-------------------------------------------------------------------------------------------------
// HttpApiOpenConnectionToTheTargetHost

class HttpApiOpenConnectionToTheTargetHost:
    public HttpApi
{
protected:
    void whenIssuedApiRequest()
    {
        using namespace std::placeholders;

        relayClient().openConnectionToTheTargetHost(
            "some_session_id",
            std::bind(&HttpApiOpenConnectionToTheTargetHost::onRequestCompletion, this, _1));
    }

    void thenRequestHasBeenDeliveredToTheManager()
    {
        m_receivedConnectToPeerRequests.pop();
    }
};

TEST_F(HttpApiOpenConnectionToTheTargetHost, request_is_delivered)
{
    whenIssuedApiRequest();
    thenRequestSucceeded();
    thenRequestHasBeenDeliveredToTheManager();
}

//-------------------------------------------------------------------------------------------------
// HttpApiStatistics

namespace {

class StatisticsProviderStub:
    public AbstractStatisticsProvider
{
public:
    virtual Statistics getAllStatistics() const override
    {
        return m_statistics;
    }

    void setStatistics(Statistics statistics)
    {
        m_statistics = statistics;
    }

private:
    Statistics m_statistics;
};

} // namespace

class HttpApiStatistics:
    public HttpApi
{
public:
    HttpApiStatistics()
    {
        m_statisticsProviderFactoryBak =
            StatisticsProviderFactory::instance().setCustomFunc(
                std::bind(&HttpApiStatistics::createStatisticsProviderStub, this));
    }

    ~HttpApiStatistics()
    {
        if (m_httpClient)
            m_httpClient->pleaseStopSync();

        if (m_statisticsProviderFactoryBak)
        {
            StatisticsProviderFactory::instance().setCustomFunc(
                std::move(*m_statisticsProviderFactoryBak));
        }
    }

protected:
    void whenRequestStatistics()
    {
        using namespace std::placeholders;

        m_httpClient = std::make_unique<GetStatisticsHttpClient>(
            nx::network::url::Builder(basicUrl())
                .setPath(api::kRelayStatisticsMetricsPath).toUrl(),
            nx::network::http::AuthInfo());
        m_httpClient->execute(
            std::bind(&HttpApiStatistics::saveStatisticsRequestResult, this, _1, _2, _3));
    }

    void andExpectedStatisticsIsProvided()
    {
        const auto prevStatistics = m_receivedStatistics.pop();
        ASSERT_EQ(m_expectedStatistics, prevStatistics);
    }

private:
    using GetStatisticsHttpClient =
        nx::network::http::FusionDataHttpClient<void, nx::cloud::relay::Statistics>;

    std::unique_ptr<GetStatisticsHttpClient> m_httpClient;
    nx::utils::SyncQueue<nx::cloud::relay::Statistics> m_receivedStatistics;
    std::optional<StatisticsProviderFactory::Function> m_statisticsProviderFactoryBak;
    Statistics m_expectedStatistics;

    std::unique_ptr<AbstractStatisticsProvider> createStatisticsProviderStub()
    {
        auto result = std::make_unique<StatisticsProviderStub>();
        m_expectedStatistics = generateRandomStatistics();
        result->setStatistics(m_expectedStatistics);
        return std::move(result);
    }

    Statistics generateRandomStatistics()
    {
        Statistics statistics;

        statistics.relaying.connectionsAcceptedPerMinute =
            nx::utils::random::number<>(1, 20);
        statistics.relaying.connectionCount = nx::utils::random::number<>(1, 20);
        statistics.relaying.connectionsAveragePerServerCount = nx::utils::random::number<>(1, 20);
        statistics.relaying.listeningServerCount = nx::utils::random::number<>(1, 20);

        statistics.http.connectionCount = nx::utils::random::number<>(1, 20);
        statistics.http.connectionsAcceptedPerMinute = nx::utils::random::number<>(1, 20);
        statistics.http.requestsAveragePerConnection = nx::utils::random::number<>(1, 20);
        statistics.http.requestsServedPerMinute = nx::utils::random::number<>(1, 20);

        return statistics;
    }

    void saveStatisticsRequestResult(
        SystemError::ErrorCode /*systemErrorCode*/,
        const nx::network::http::Response* response,
        nx::cloud::relay::Statistics statistics)
    {
        onRequestCompletion(
            response
            ? api::fromHttpStatusCode(static_cast<nx::network::http::StatusCode::Value>(
                response->statusLine.statusCode))
            : api::ResultCode::networkError);
        m_receivedStatistics.push(std::move(statistics));
    }
};

TEST_F(HttpApiStatistics, statistics_provided)
{
    whenRequestStatistics();

    thenRequestSucceeded();
    andExpectedStatisticsIsProvided();
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
