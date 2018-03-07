#pragma once

#include <chrono>
#include <list>
#include <memory>
#include <string>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/abstract_socket.h>
#include <nx/network/aio/abstract_async_channel.h>
#include <nx/network/aio/async_channel_bridge.h>
#include <nx/utils/data_structures/top_queue.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/statistics/top_value_per_period_calculator.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace cloud {
namespace relay {
namespace controller {

struct RelayConnectionData
{
    std::unique_ptr<network::aio::AbstractAsyncChannel> connection;
    std::string peerId;
};

struct RelaySessionStatistics
{
    int currentSessionCount = 0;
    int concurrentSessionToSameServerCountMaxPerHour = 0;
    int concurrentSessionToSameServerCountAveragePerHour = 0;
    /** Sessions that were terminated during last hour are counted here. */
    int sessionDurationSecAveragePerLastHour = 0;
    /** Sessions that were terminated during last hour are counted here. */
    int sessionDurationSecMaxPerLastHour = 0;
};

#define RelaySessionStatistics_controller_Fields \
    (currentSessionCount) \
    (concurrentSessionToSameServerCountMaxPerHour) \
    (concurrentSessionToSameServerCountAveragePerHour) \
    (sessionDurationSecAveragePerLastHour) \
    (sessionDurationSecMaxPerLastHour)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (RelaySessionStatistics),
    (json))

/**
 * NOTE: Not thread-safe.
 */
class TrafficRelayStatisticsCollector
{
public:
    TrafficRelayStatisticsCollector();

    void onSessionStarted(const std::string& id);
    void onSessionStopped(
        const std::string& id,
        const std::chrono::milliseconds duration);

    RelaySessionStatistics getStatistics() const;

private:
    int m_currentSessionCount = 0;
    std::map<std::string, int> m_serverIdToCurrentSessionCount;
    nx::utils::statistics::MaxValuePerPeriodCalculator<int> m_maxSessionCountPerPeriodCalculator;
    nx::utils::statistics::MaxValuePerPeriodCalculator<std::chrono::seconds>
        m_maxSessionDurationCalculator;
};

//-------------------------------------------------------------------------------------------------

class AbstractTrafficRelay
{
public:
    virtual ~AbstractTrafficRelay() = default;

    virtual void startRelaying(
        RelayConnectionData clientConnection,
        RelayConnectionData serverConnection) = 0;

    virtual RelaySessionStatistics getStatistics() const = 0;
};

class TrafficRelay:
    public AbstractTrafficRelay
{
public:
    virtual ~TrafficRelay() override;

    virtual void startRelaying(
        RelayConnectionData clientConnection,
        RelayConnectionData serverConnection) override;

    virtual RelaySessionStatistics getStatistics() const override;

private:
    struct RelaySession
    {
        std::unique_ptr<network::aio::AsyncChannelBridge> channelBridge;
        std::string clientPeerId;
        std::string serverPeerId;
        std::chrono::steady_clock::time_point startTime;
    };

    std::list<RelaySession> m_relaySessions;
    mutable QnMutex m_mutex;
    bool m_terminated = false;
    TrafficRelayStatisticsCollector m_statisticsCollector;

    void onRelaySessionFinished(std::list<RelaySession>::iterator sessionIter);
};

} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
