#include <gtest/gtest.h>

#include <nx/utils/random.h>

#include <nx/cloud/relay/statistics_provider.h>
#include <nx/cloud/relaying/listening_peer_pool.h>

#include "statistics_provider_ut.h"

namespace nx {
namespace cloud {
namespace relay {
namespace test {

relaying::Statistics generateRelayingStatistics()
{
    relaying::Statistics statistics;
    statistics.connectionsAcceptedPerMinute =
        nx::utils::random::number<>(1, 20);
    statistics.connectionCount = nx::utils::random::number<>(1, 20);
    statistics.connectionsAveragePerServerCount = nx::utils::random::number<>(1, 20);
    statistics.listeningServerCount = nx::utils::random::number<>(1, 20);
    return statistics;
}

controller::RelaySessionStatistics generateRelaySessionStatistics()
{
    controller::RelaySessionStatistics statistics;
    return statistics;
}

nx::network::server::Statistics generateHttpServerStatistics()
{
    nx::network::server::Statistics statistics;
    statistics.connectionCount = nx::utils::random::number<>(1, 20);
    statistics.connectionsAcceptedPerMinute = nx::utils::random::number<>(1, 20);
    statistics.requestsAveragePerConnection = nx::utils::random::number<>(1, 20);
    statistics.requestsServedPerMinute = nx::utils::random::number<>(1, 20);
    return statistics;
}

//-------------------------------------------------------------------------------------------------

namespace {

class ListeningPeerPoolStub:
    public relaying::ListeningPeerPool
{
public:
    ListeningPeerPoolStub(const relaying::Settings& settings):
        relaying::ListeningPeerPool(settings)
    {
    }

    virtual relaying::Statistics statistics() const override
    {
        m_lastProvidedStatistics = generateRelayingStatistics();
        return m_lastProvidedStatistics;
    }

    relaying::Statistics lastProvidedStatistics() const
    {
        return m_lastProvidedStatistics;
    }

private:
    mutable relaying::Statistics m_lastProvidedStatistics;
};

//-------------------------------------------------------------------------------------------------

class TrafficRelayStub:
    public controller::AbstractTrafficRelay
{
public:
    virtual void startRelaying(
        controller::RelayConnectionData /*clientConnection*/,
        controller::RelayConnectionData /*serverConnection*/) override
    {
    }

    virtual controller::RelaySessionStatistics statistics() const override
    {
        m_lastProvidedStatistics = generateRelaySessionStatistics();
        return m_lastProvidedStatistics;
    }

    controller::RelaySessionStatistics lastProvidedStatistics() const
    {
        return m_lastProvidedStatistics;
    }

private:
    mutable controller::RelaySessionStatistics m_lastProvidedStatistics;
};

//-------------------------------------------------------------------------------------------------

class HttpServerStatisticsProvider:
    public nx::network::server::AbstractStatisticsProvider
{
public:
    virtual nx::network::server::Statistics statistics() const override
    {
        m_lastProvidedStatistics = generateHttpServerStatistics();
        return m_lastProvidedStatistics;
    }

    nx::network::server::Statistics lastProvidedStatistics() const
    {
        return m_lastProvidedStatistics;
    }

private:
    mutable nx::network::server::Statistics m_lastProvidedStatistics;
};

} // namespace

//-------------------------------------------------------------------------------------------------

class StatisticsProvider:
    public ::testing::Test
{
public:
    StatisticsProvider():
        m_listeningPeerPool(m_settings),
        m_statisticsProvider(
            m_listeningPeerPool,
            m_httpServerStatisticsProvider,
            m_trafficRelay)
    {
    }

protected:
    relaying::Settings m_settings;
    ListeningPeerPoolStub m_listeningPeerPool;
    HttpServerStatisticsProvider m_httpServerStatisticsProvider;
    TrafficRelayStub m_trafficRelay;

    relay::StatisticsProvider m_statisticsProvider;
};

TEST_F(StatisticsProvider, reads_listening_peer_pool_statistics)
{
    const auto statistics = m_statisticsProvider.getAllStatistics();
    ASSERT_EQ(statistics.relaying, m_listeningPeerPool.lastProvidedStatistics());
}

TEST_F(StatisticsProvider, reads_http_server_statistics)
{
    const auto statistics = m_statisticsProvider.getAllStatistics();
    ASSERT_EQ(statistics.http, m_httpServerStatisticsProvider.lastProvidedStatistics());
}

TEST_F(StatisticsProvider, reads_traffic_relay_statistics)
{
    const auto statistics = m_statisticsProvider.getAllStatistics();
    ASSERT_EQ(statistics.relaySessions, m_trafficRelay.lastProvidedStatistics());
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
