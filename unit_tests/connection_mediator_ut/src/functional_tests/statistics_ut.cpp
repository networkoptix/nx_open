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

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        ASSERT_TRUE(startAndWaitUntilStarted());
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

    void thenStatisticsIsProvided()
    {
        ASSERT_TRUE(static_cast<bool>(m_responseBody));

        bool isSucceeded = false;
        m_serverStatistics = QJson::deserialized<stats::Statistics>(
            *m_responseBody, stats::Statistics(), &isSucceeded);
        ASSERT_TRUE(isSucceeded);
    }

private:
    boost::optional<nx::network::http::StringType> m_responseBody;
    stats::Statistics m_serverStatistics;
};

TEST_F(StatisticsApi, listening_peer_list)
{
    const auto client = clientConnection();
    auto clientGuard = makeScopeGuard([&client]() { client->pleaseStopSync(); });
    const auto system1 = addRandomSystem();
    auto server1 = addServer(system1, QnUuid::createUuid().toSimpleString().toUtf8());

    hpm::api::ClientBindRequest request;
    request.originatingPeerID = "someClient";
    request.tcpReverseEndpoints.push_back(nx::network::SocketAddress("12.34.56.78:1234"));
    nx::utils::promise<void> bindPromise;
    client->send(
        request,
        [&](nx::hpm::api::ResultCode code)
        {
            ASSERT_EQ(code, nx::hpm::api::ResultCode::ok);
            bindPromise.set_value();
        });
    bindPromise.get_future().wait();

    nx::network::http::StatusCode::Value statusCode = nx::network::http::StatusCode::ok;
    api::ListeningPeers listeningPeers;
    std::tie(statusCode, listeningPeers) = getListeningPeers();
    ASSERT_EQ(nx::network::http::StatusCode::ok, statusCode);

    ASSERT_EQ(1U, listeningPeers.systems.size());
    const auto systemIter = listeningPeers.systems.find(system1.id);
    ASSERT_NE(listeningPeers.systems.end(), systemIter);

    ASSERT_EQ(1U, systemIter->second.size());
    const auto serverIter = systemIter->second.find(server1->serverId());
    ASSERT_NE(systemIter->second.end(), serverIter);

    ASSERT_EQ(1U, listeningPeers.clients.size());
    const auto& boundClient = *listeningPeers.clients.begin();
    ASSERT_EQ("someClient", boundClient.first);
    ASSERT_EQ(1U, boundClient.second.tcpReverseEndpoints.size());
    ASSERT_EQ(nx::network::SocketAddress("12.34.56.78:1234"), boundClient.second.tcpReverseEndpoints.front());
}

TEST_F(StatisticsApi, http_and_stun_server_statistics)
{
    whenRequestServerStatistics();

    thenStatisticsIsProvided();
}

} // namespace test
} // namespace hpm
} // namespace nx
