#include <atomic>

#include <thread>

#include <gtest/gtest.h>

#include <nx/network/aio/test/aio_test_async_channel.h>
#include <nx/utils/uuid.h>
#include <nx/utils/byte_stream/pipeline.h>
#include <nx/utils/random.h>
#include <nx/utils/string.h>
#include <nx/utils/test_support/test_pipeline.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/time.h>

#include <nx/cloud/relay/controller/traffic_relay.h>

namespace nx {
namespace cloud {
namespace relay {
namespace controller {
namespace test {

namespace {

class AsyncChannel:
    public network::aio::test::AsyncChannel
{
    using base_type = network::aio::test::AsyncChannel;

public:
    AsyncChannel(
        utils::bstream::AbstractInput* input,
        utils::bstream::AbstractOutput* output)
        :
        base_type(input, output, InputDepletionPolicy::retry)
    {
    }

    ~AsyncChannel()
    {
        if (m_channelClosedEventQueue)
            m_channelClosedEventQueue->push(this);
    }

    void setChannelClosedEventQueue(nx::utils::SyncQueue<AsyncChannel*>* eventQueue)
    {
        m_channelClosedEventQueue = eventQueue;
    }

private:
    nx::utils::SyncQueue<AsyncChannel*>* m_channelClosedEventQueue = nullptr;
};

} // namespace

//-------------------------------------------------------------------------------------------------

class TrafficRelay:
    public ::testing::Test
{
public:
    TrafficRelay()
    {
    }

protected:
    struct ChannelContext
    {
        utils::bstream::Pipe input;
        utils::bstream::test::NotifyingOutput output;
        std::unique_ptr<AsyncChannel> channel;
        AsyncChannel* channelPtr;

        ChannelContext():
            channel(std::make_unique<AsyncChannel>(&input, &output)),
            channelPtr(channel.get())
        {
        }
    };

    struct RelaySessionContext
    {
        std::unique_ptr<ChannelContext> clientChannel;
        std::unique_ptr<ChannelContext> serverChannel;

        RelaySessionContext():
            clientChannel(std::make_unique<ChannelContext>()),
            serverChannel(std::make_unique<ChannelContext>())
        {
        }
    };

    void addRelaySession(std::string serverId = std::string())
    {
        if (serverId.empty())
            serverId = QnUuid::createUuid().toSimpleString().toStdString();

        m_relaySessions.push_back(RelaySessionContext{});
        m_relaySessions.back().clientChannel->channel->setChannelClosedEventQueue(
            &m_channelClosedEventQueue);
        m_relaySessions.back().serverChannel->channel->setChannelClosedEventQueue(
            &m_channelClosedEventQueue);

        m_trafficRelay.startRelaying(
            {std::move(m_relaySessions.back().clientChannel->channel), ""},
            {std::move(m_relaySessions.back().serverChannel->channel), serverId});
    }

    void whenEitherSocketIsClosed()
    {
        RelaySessionContext& relaySessionContext = m_relaySessions.back();

        AsyncChannel* channelToClose = nullptr;
        if (nx::utils::random::number<int>(0, 1) > 0)
            channelToClose = relaySessionContext.clientChannel->channelPtr;
        else
            channelToClose = relaySessionContext.serverChannel->channelPtr;
        channelToClose->setReadErrorState(SystemError::connectionReset);
    }

    void thenTrafficBetweenStreamSocketsIsRelayed()
    {
        RelaySessionContext& relaySessionContext = m_relaySessions.back();

        const QByteArray testMessage = nx::utils::random::generate(
            nx::utils::random::number(4096, 128*1024));

        relaySessionContext.clientChannel->input.write(testMessage.data(), testMessage.size());
        relaySessionContext.serverChannel->output.waitForReceivedDataToMatch(testMessage);
    }

    void thenTrafficRelayClosesBothChannels()
    {
        RelaySessionContext& relaySessionContext = m_relaySessions.back();

        for (int i = 0; i < 2; ++i)
        {
            auto closedChannelPtr = m_channelClosedEventQueue.pop();
            ASSERT_TRUE(
                closedChannelPtr == relaySessionContext.clientChannel->channelPtr ||
                closedChannelPtr == relaySessionContext.serverChannel->channelPtr);
        }

        m_relaySessions.pop_back();
    }

    std::vector<TrafficRelay::RelaySessionContext>& relaySessions()
    {
        return m_relaySessions;
    }

    controller::TrafficRelay& trafficRelay()
    {
        return m_trafficRelay;
    }

private:
    nx::utils::SyncQueue<AsyncChannel*> m_channelClosedEventQueue;
    std::vector<RelaySessionContext> m_relaySessions;
    controller::TrafficRelay m_trafficRelay;
};

TEST_F(TrafficRelay, actually_relays_data)
{
    addRelaySession();
    thenTrafficBetweenStreamSocketsIsRelayed();
}

TEST_F(TrafficRelay, relaying_is_stopped_with_either_connection_closure)
{
    addRelaySession();
    whenEitherSocketIsClosed();
    thenTrafficRelayClosesBothChannels();
}

//-------------------------------------------------------------------------------------------------

class TrafficRelayStatistics:
    public TrafficRelay
{
public:
    TrafficRelayStatistics():
        m_timeShift(nx::utils::test::ClockType::steady)
    {
    }

protected:
    void createMultipleSessions()
    {
        createMultipleSessions(QnUuid::createUuid().toSimpleString().toStdString(), 7);
    }

    void createMultipleSessionsToDifferentServers()
    {
        m_serverIds.resize(3);
        std::generate(
            m_serverIds.begin(), m_serverIds.end(),
            []() { return QnUuid::createUuid().toSimpleString().toStdString(); });

        for (const auto& serverId: m_serverIds)
        {
            const auto sessionCount = nx::utils::random::number<int>(2, 11);
            createMultipleSessions(serverId, sessionCount);
        }
    }

    void waitForSomeTime()
    {
        m_timeShift.applyRelativeShift(std::chrono::minutes(1));
        m_minExpectedSessionDuration += std::chrono::minutes(1);
    }

    void terminateAllSessions()
    {
        const int sessionCount = relaySessions().size();
        for (int i = 0; i < sessionCount; ++i)
        {
            whenEitherSocketIsClosed();
            thenTrafficRelayClosesBothChannels();
        }
    }

    void assertStatisticsProvidedIsCorrect()
    {
        using namespace std::chrono;

        const auto statistics = trafficRelay().statistics();

        // Current session count.
        ASSERT_EQ(relaySessions().size(), (std::size_t)statistics.currentSessionCount);

        // Max session count per server.
        const auto maxSessionCountPerServer = std::max_element(
            m_totalSessionCountByServer.begin(), m_totalSessionCountByServer.end(),
            [](const auto& left, const auto& right)
            {
                return left.second < right.second;
            })->second;
        ASSERT_EQ(
            maxSessionCountPerServer,
            statistics.concurrentSessionToSameServerCountMaxPerHour);

        ASSERT_GT(statistics.concurrentSessionToSameServerCountAveragePerHour, 0);

        // sessionDurationSecMaxPerLastHour.
        ASSERT_GE(
            statistics.sessionDurationSecMaxPerLastHour,
            duration_cast<seconds>(m_minExpectedSessionDuration).count());

        if (m_minExpectedSessionDuration > std::chrono::seconds::zero())
        {
            ASSERT_GE(
                statistics.sessionDurationSecAveragePerLastHour,
                duration_cast<seconds>(m_minExpectedSessionDuration).count());
        }
    }

private:
    int m_totalSessions = 0;
    std::vector<std::string> m_serverIds;
    std::map<std::string, int> m_totalSessionCountByServer;
    nx::utils::test::ScopedTimeShift m_timeShift;
    std::chrono::seconds m_minExpectedSessionDuration = std::chrono::seconds::zero();

    void createMultipleSessions(const std::string& serverId, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            addRelaySession(serverId);
            ++m_totalSessions;
            ++m_totalSessionCountByServer[serverId];
        }
    }
};

TEST_F(TrafficRelayStatistics, active_session_count)
{
    createMultipleSessions();
    assertStatisticsProvidedIsCorrect();
}

TEST_F(TrafficRelayStatistics, closed_sessions_are_reflected_in_statistics)
{
    createMultipleSessions();
    terminateAllSessions();

    assertStatisticsProvidedIsCorrect();
}

TEST_F(TrafficRelayStatistics, max_session_count_per_server)
{
    createMultipleSessionsToDifferentServers();
    terminateAllSessions();

    assertStatisticsProvidedIsCorrect();
}

TEST_F(TrafficRelayStatistics, max_session_duration)
{
    createMultipleSessions();
    waitForSomeTime();
    terminateAllSessions();

    assertStatisticsProvidedIsCorrect();
}

} // namespace test
} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
