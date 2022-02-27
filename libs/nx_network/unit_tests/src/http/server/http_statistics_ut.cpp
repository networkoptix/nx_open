// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/network/http/server/http_statistics.h>

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
        for(char i  = '0'; i < '7'; ++i)
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



} // namespace nx::network::http::server::test