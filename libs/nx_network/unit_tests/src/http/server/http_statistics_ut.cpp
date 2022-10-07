// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/http/abstract_msg_body_source.h> //< Needs to include before request_processing_types.h
#include <nx/network/http/http_client.h>
#include <nx/network/http/server/http_server_builder.h>
#include <nx/network/http/server/multi_endpoint_server.h>
#include <nx/network/http/server/request_processing_types.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/network/http/server/settings.h>
#include <nx/network/system_socket.h>
#include <nx/network/url/url_builder.h>
#include <nx/reflect/json.h>
#include <nx/utils/time.h>
#include <nx/utils/random.h>
#include <nx/utils/test_support/utils.h>

namespace nx::network::http::server::test {

using namespace std::chrono;

namespace {

class HttpStatisticsProviderStub:
    public AbstractHttpStatisticsProvider
{
public:
    HttpStatisticsProviderStub()
    {
    }

    void setHttpStatistics(HttpStatistics stats)
    {
        m_stats = std::move(stats);
    }

    virtual HttpStatistics httpStatistics() const override
    {
        return m_stats;
    }

private:
    HttpStatistics m_stats;
};

} //namespace

class AggregateHttpStatisticsProvider:
    public testing::Test
{
protected:
    virtual void SetUp() override
    {
        for (char i  = '0'; i < '7'; ++i)
            m_requestPaths.emplace_back("/") += i;
    }

    HttpStatistics buildStatistics(int requestPathIndex)
    {
        HttpStatistics stats;
        stats.connectionCount = 1;
        stats.connectionsAcceptedPerMinute = 1;
        stats.requestsServedPerMinute = 1;
        stats.requestsAveragePerConnection = 1;
        stats.notFound404 = 1;

        auto& requestPathStats = stats.requests[m_requestPaths[requestPathIndex]];
        // Has a value from 1 to m_requestPaths.size()
        requestPathStats.averageRequestProcessingTimeUsec =
            std::chrono::microseconds(requestPathIndex + 1);
        requestPathStats.maxRequestProcessingTimeUsec =
            std::chrono::microseconds(requestPathIndex + 1);
        requestPathStats.requestsServedPerMinute = 1;

        // Making sure the aggregate max time matches the request path max time
        stats.maxRequestProcessingTimeUsec = requestPathStats.maxRequestProcessingTimeUsec;
        stats.averageRequestProcessingTimeUsec = requestPathStats.averageRequestProcessingTimeUsec;

        m_averageRequestProcessingTimeSum += stats.averageRequestProcessingTimeUsec;

        return stats;
    }

    std::vector<HttpStatisticsProviderStub> givenStatisticsProviders()
    {
        std::vector<HttpStatisticsProviderStub> providers;
        for (std::size_t i = 0; i < m_requestPaths.size(); ++i)
            providers.emplace_back().setHttpStatistics(buildStatistics(i));
        return providers;
    }

    std::vector<HttpStatisticsProviderStub> givenStastisticsProvidersWithOverlappingRequestPaths()
    {
        auto providers = givenStatisticsProviders();

        auto statsFront = providers.front().httpStatistics();
        auto statsBack = providers.back().httpStatistics();

        const auto itFront = statsFront.requests.begin();
        statsBack.requests.emplace(itFront->first, itFront->second);
        providers.back().setHttpStatistics(statsBack);

        const auto itBack = statsBack.requests.begin();
        statsFront.requests.emplace(itBack->first, itBack->second);
        providers.front().setHttpStatistics(statsFront);

        return providers;
    }

    HttpStatistics whenRequestAggregateStatistics(
        const std::vector<HttpStatisticsProviderStub>& providers)
    {
        std::vector<const AbstractHttpStatisticsProvider*> abstractProviders;
        for (const auto& provider: providers)
            abstractProviders.emplace_back(&provider);
        return server::AggregateHttpStatisticsProvider(
            std::move(abstractProviders)).httpStatistics();
    }

    void thenStatisticsHaveExpectedValues(
        const HttpStatistics& expected,
        const HttpStatistics& actual)
    {
        ASSERT_EQ(expected.connectionCount, actual.connectionCount);
        ASSERT_EQ(expected.connectionsAcceptedPerMinute, actual.connectionsAcceptedPerMinute);
        ASSERT_EQ(expected.requestsServedPerMinute, actual.requestsServedPerMinute);
        ASSERT_EQ(expected.requestsAveragePerConnection, actual.requestsAveragePerConnection);
        ASSERT_EQ(expected.notFound404, actual.notFound404);

        ASSERT_EQ(expected.requests.size(), actual.requests.size());
        for(const auto& [path, expectedStats]: expected.requests)
        {
            const auto it = actual.requests.find(path);
            ASSERT_NE(actual.requests.end(), it);
            ASSERT_EQ(expectedStats.requestsServedPerMinute, it->second.requestsServedPerMinute);
            ASSERT_EQ(
                expectedStats.averageRequestProcessingTimeUsec,
                it->second.averageRequestProcessingTimeUsec);
            ASSERT_EQ(
                expectedStats.maxRequestProcessingTimeUsec,
                it->second.maxRequestProcessingTimeUsec);
        }
    }

    HttpStatistics expectedStatistics(const std::vector<HttpStatisticsProviderStub>& providers)
    {
        HttpStatistics all;
        std::map<
            std::string /*requestPath*/,
            std::pair<
                int /*count*/,
                std::chrono::microseconds /*averageRequestProcessingTimeUsecSum*/>
        > averageRequestProcessingTimeContext;

        all.connectionCount = (int) providers.size();
        all.connectionsAcceptedPerMinute = (int) providers.size();
        all.requestsServedPerMinute = (int) providers.size();
        all.requestsAveragePerConnection = (int) providers.size();
        all.averageRequestProcessingTimeUsec =
            m_averageRequestProcessingTimeSum / (int) providers.size();
        all.maxRequestProcessingTimeUsec = std::chrono::microseconds((int) providers.size());
        all.notFound404 = (int) providers.size();

        for (const auto& provider: providers)
        {
            const auto stats = provider.httpStatistics();
            for (const auto& [path, pathStats]: stats.requests)
            {
                auto& allPathStats = all.requests[path];
                allPathStats.requestsServedPerMinute += pathStats.requestsServedPerMinute;
                allPathStats.maxRequestProcessingTimeUsec = std::max(
                    allPathStats.maxRequestProcessingTimeUsec,
                    pathStats.maxRequestProcessingTimeUsec);

                auto& context = averageRequestProcessingTimeContext[path];
                ++context.first;
                context.second += pathStats.averageRequestProcessingTimeUsec;
            }
        }

        for(const auto& [path, context]: averageRequestProcessingTimeContext)
            all.requests[path].averageRequestProcessingTimeUsec = context.second / context.first;

        return all;
    }

private:
    std::vector<std::string> m_requestPaths;
    std::chrono::microseconds m_averageRequestProcessingTimeSum{0};
};

TEST_F(AggregateHttpStatisticsProvider, aggregates_statistics)
{
    const auto providers = givenStatisticsProviders();

    const auto statistics = whenRequestAggregateStatistics(providers);

    thenStatisticsHaveExpectedValues(expectedStatistics(providers), statistics);
}

TEST_F(AggregateHttpStatisticsProvider, merges_request_path_statistics)
{
    const auto providers = givenStastisticsProvidersWithOverlappingRequestPaths();

    const auto statistics = whenRequestAggregateStatistics(providers);

    thenStatisticsHaveExpectedValues(expectedStatistics(providers), statistics);
}

// ------------------------------------------------------------------------------------------------
// MultiEndpointServerHttpStatistics

class MultiEndpointServerHttpStatistics: public testing::Test
{
public:
    void SetUp()
    {
        http::server::Settings settings;
        settings.endpoints.emplace_back(SocketAddress::anyPrivateAddressV4);
        m_httpServer = http::server::Builder::buildOrThrow(settings, nullptr, &m_httpDispatcher);
        ASSERT_TRUE(m_httpServer->listen());
    }

    void givenGetRequestPaths(std::vector<std::string> requestPaths)
    {
        for (const auto& path: requestPaths)
        {
            m_httpDispatcher.registerRequestProcessorFunc(
                http::Method::get,
                path,
                [](auto /* requestContext */, auto handler){ handler(http::StatusCode::ok); });
        }
    }

    void whenMakeGetRequestWithExistingClient(http::HttpClient* client, std::string path)
    {
        ASSERT_TRUE(client->doGet(url::Builder(m_httpServer->preferredUrl()).setPath(path)));
    }

    void whenMakeGetRequest(std::string path)
    {
        http::HttpClient client(ssl::kAcceptAnyCertificate);
        whenMakeGetRequestWithExistingClient(&client, path);
    }

    std::unique_ptr<AsyncMessagePipeline> makeHttpClientSocket()
    {
        auto socket = std::make_unique<TCPSocket>();
        NX_GTEST_ASSERT_TRUE(
            socket->connect(m_httpServer->endpoints().front(), std::chrono::minutes(10)));

        return std::make_unique<AsyncMessagePipeline>(std::move(socket));
    }

    void doGetRequestsOnOneSocket(AsyncMessagePipeline* socket, std::string path, std::size_t count)
    {
        std::atomic_uint responses = 0;
        socket->setMessageHandler(
            [&responses](auto message)
            {
                ++responses;
                ASSERT_EQ(StatusCode::ok, message.response->statusLine.statusCode);
            });

        socket->startReadingConnection();

        http::Request r;
        r.requestLine.method = Method::get;
        r.requestLine.url.setPath(path);
        r.requestLine.version = http_1_1;
        const auto buffer = r.serialized();

        for (std::size_t i = 0; i < count; ++i)
        {
            socket->sendData(
                buffer,
                [](auto errorCode){ ASSERT_EQ(SystemError::noError, errorCode); });
        }

        while (responses < count)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    void whenMakeAsyncGetRequest(
        std::string path,
        nx::utils::MoveOnlyFunc<void()> handler)
    {
        auto client = std::make_unique<http::AsyncClient>(ssl::kAcceptAnyCertificate);
        client->setTimeouts(AsyncClient::kInfiniteTimeouts);
        auto clientPtr = client.get();

        clientPtr->doGet(
            url::Builder(m_httpServer->preferredUrl()).setPath(path),
            [client = std::move(client), handler = std::move(handler)]() mutable
            {
                auto f = std::move(handler); //< handler is freed with lambda by client.reset().
                client.reset();
                f();
            });
    }

    http::server::HttpStatistics whenRequestHttpStatistics()
    {
        return m_httpServer->httpStatistics();
    }

private:
    http::server::rest::MessageDispatcher m_httpDispatcher;
    std::unique_ptr<http::server::MultiEndpointServer> m_httpServer;
};

TEST_F(MultiEndpointServerHttpStatistics, RequestPathStatistics_present_if_requestsServedPerMinute_is_not_0)
{
    givenGetRequestPaths({"/0", "/1"});

    whenMakeGetRequest("/0");

    auto stats = whenRequestHttpStatistics();
    // Ensure "GET /0" is present before one minute passes.
    ASSERT_NE(stats.requests.end(), stats.requests.find("GET /0"));

    // Using ClockType::steady because RequestPathStatisticsCalculator uses
    // nx::utils::math::MaxPerMinute which uses std::chrono::steady_clock.
    nx::utils::test::ScopedTimeShift timeShift(
        nx::utils::test::ClockType::steady,
        std::chrono::minutes(2));

    whenMakeGetRequest("/1");

    stats = whenRequestHttpStatistics();

    ASSERT_EQ(stats.requests.end(), stats.requests.find("GET /0"));
    ASSERT_EQ(1, stats.requests.find("GET /1")->second.requestsServedPerMinute);
}

TEST_F(MultiEndpointServerHttpStatistics,
RequestPathStatistics_requestsServedPerMinute_matches_aggregate_requestsServerPerMinute_while_connections_are_open)
{
    givenGetRequestPaths({"/0", "/1"});

    auto socket = makeHttpClientSocket();
    std::thread t{
        [this, &socket]()
        {
            doGetRequestsOnOneSocket(socket.get(), "/1", 101);
        }};

    auto socket2 = makeHttpClientSocket();
    doGetRequestsOnOneSocket(socket2.get(), "/0", 99);

    t.join(); //<Wait for socket to complete its operations in background thread.

    const auto stats = whenRequestHttpStatistics();

    int requestPathStatsRequestsServedPerMinute = 0;
    for (const auto& requestPathStats: stats.requests)
        requestPathStatsRequestsServedPerMinute += requestPathStats.second.requestsServedPerMinute;

    ASSERT_EQ(200, stats.requestsServedPerMinute);
    ASSERT_EQ(stats.requestsServedPerMinute, requestPathStatsRequestsServedPerMinute);

    // Sockets must be closed after statistics are taken, because the must report correct values
    // while sockets are still alive.
    socket->pleaseStopSync();
    socket2->pleaseStopSync();
}

TEST_F(MultiEndpointServerHttpStatistics, percentiles_present)
{
    givenGetRequestPaths({"/0"});

    nx::utils::test::ScopedSyntheticMonotonicTime fixedTime{};

    std::atomic_int requestsHandled = 0;
    for (int i = 0; i < 100; ++i)
    {
        whenMakeAsyncGetRequest(
            "/0",
            [&requestsHandled]() { ++requestsHandled; });
    }

    while (requestsHandled != 100)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    // Need to advance time past initial collection period to trigger value caching in the
    // PercentilePerPeriod instances.
    fixedTime.applyRelativeShift(std::chrono::minutes(1));

    auto stats = whenRequestHttpStatistics();

    const auto zero = std::chrono::microseconds::zero();
    ASSERT_NE(zero, stats.requestProcessingTimePercentilesUsec.at("50"));
    ASSERT_NE(zero, stats.requestProcessingTimePercentilesUsec.at("95"));
    ASSERT_NE(zero, stats.requestProcessingTimePercentilesUsec.at("99"));
}

} // namespace nx::network::http::server::test
