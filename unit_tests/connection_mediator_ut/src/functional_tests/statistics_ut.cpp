#include <future>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <nx/fusion/serialization/json.h>
#include <nx/network/cloud/data/client_bind_data.h>
#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>
#include <nx/network/http/http_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/string.h>

#include <mediator_service.h>
#include <statistics/statistics_provider.h>

#include "functional_tests/mediator_functional_test.h"

namespace nx {
namespace hpm {
namespace test {

class StatisticsApi:
    public MediatorFunctionalTest
{
    using base_type = MediatorFunctionalTest;

public:
    ~StatisticsApi()
    {
        if (m_clientConnection)
            m_clientConnection->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        ASSERT_TRUE(startAndWaitUntilStarted());

        m_system = addRandomSystem();
        m_server = addServer(m_system, QnUuid::createUuid().toSimpleString().toUtf8());
    }

    void whenRequestListeningPeerList()
    {
        nx::network::http::StatusCode::Value statusCode =
            nx::network::http::StatusCode::ok;
        std::tie(statusCode, m_prevListeningPeersResponse) = getListeningPeers();
        ASSERT_EQ(nx::network::http::StatusCode::ok, statusCode);
    }

    void whenRequestServerStatistics()
    {
        const auto statisticsUrl =
            nx::network::url::Builder()
            .setScheme(nx::network::http::kUrlSchemeName)
            .setEndpoint(moduleInstance()->impl()->httpEndpoints().front())
            .appendPath(api::kMediatorApiPrefix).appendPath(api::kStatisticsMetricsPath).toUrl();

        nx::network::http::HttpClient httpClient;
        ASSERT_TRUE(httpClient.doGet(statisticsUrl));
        m_responseBody = httpClient.fetchEntireMessageBody();
    }

    void establishClientConnection()
    {
        m_clientConnection = clientConnection();

        hpm::api::ClientBindRequest request;
        request.originatingPeerID = "someClient";
        request.tcpReverseEndpoints.push_back(
            nx::network::SocketAddress("12.34.56.78:1234"));
        std::promise<void> bindPromise;
        m_clientConnection->send(
            request,
            [&](nx::hpm::api::ResultCode code)
            {
                ASSERT_EQ(code, nx::hpm::api::ResultCode::ok);
                bindPromise.set_value();
            });
        bindPromise.get_future().wait();
    }

    void thenExpectedConnectionsHaveBeenReported()
    {
        ASSERT_EQ(1U, m_prevListeningPeersResponse.systems.size());
        const auto systemIter = m_prevListeningPeersResponse.systems.find(m_system.id);
        ASSERT_NE(m_prevListeningPeersResponse.systems.end(), systemIter);

        ASSERT_EQ(1U, systemIter->second.size());
        const auto serverIter = systemIter->second.find(m_server->serverId());
        ASSERT_NE(systemIter->second.end(), serverIter);

        ASSERT_EQ(1U, m_prevListeningPeersResponse.clients.size());
        const auto& boundClient = *m_prevListeningPeersResponse.clients.begin();
        ASSERT_EQ("someClient", boundClient.first);
        ASSERT_EQ(1U, boundClient.second.tcpReverseEndpoints.size());
        ASSERT_EQ(
            nx::network::SocketAddress("12.34.56.78:1234"),
            boundClient.second.tcpReverseEndpoints.front());
    }

    void thenServerStatisticsIsProvided()
    {
        ASSERT_TRUE(static_cast<bool>(m_responseBody));

        bool isSucceeded = false;
        m_serverStatistics = QJson::deserialized<stats::Statistics>(
            *m_responseBody, stats::Statistics(), &isSucceeded);
        ASSERT_TRUE(isSucceeded);
    }

    void andStatisticsContainsExpectedValues()
    {
        // There was at least one connection at the request moment (connection of the request itself).
        ASSERT_GT(m_serverStatistics.http.connectionCount, 0);
        ASSERT_GT(m_serverStatistics.stun.connectionCount, 0);
        ASSERT_GT(m_serverStatistics.cloudConnect.serverCount, 0);
    }

private:
    std::optional<nx::network::http::StringType> m_responseBody;
    stats::Statistics m_serverStatistics;
    api::ListeningPeers m_prevListeningPeersResponse;
    AbstractCloudDataProvider::System m_system;
    std::unique_ptr<MediaServerEmulator> m_server;
    std::unique_ptr<api::MediatorClientTcpConnection> m_clientConnection;
};

TEST_F(StatisticsApi, listening_peer_list)
{
    establishClientConnection();
    whenRequestListeningPeerList();
    thenExpectedConnectionsHaveBeenReported();
}

TEST_F(StatisticsApi, http_and_stun_server_statistics)
{
    whenRequestServerStatistics();

    thenServerStatisticsIsProvided();
    andStatisticsContainsExpectedValues();
}

} // namespace test
} // namespace hpm
} // namespace nx
