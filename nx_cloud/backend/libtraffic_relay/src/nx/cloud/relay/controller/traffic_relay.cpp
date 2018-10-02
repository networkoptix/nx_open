#include "traffic_relay.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/aio/aio_service.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/time.h>

namespace nx {
namespace cloud {
namespace relay {
namespace controller {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (RelaySessionStatistics),
    (json),
    _controller_Fields)

//-------------------------------------------------------------------------------------------------

namespace {
static constexpr auto kSessionStatisticsAggregationPeriod = std::chrono::hours(1);
} // namespace

bool RelaySessionStatistics::operator==(const RelaySessionStatistics& right) const
{
    return currentSessionCount == right.currentSessionCount
        && concurrentSessionToSameServerCountMaxPerHour == right.concurrentSessionToSameServerCountMaxPerHour
        && concurrentSessionToSameServerCountAveragePerHour == right.concurrentSessionToSameServerCountAveragePerHour
        && sessionDurationSecAveragePerLastHour == right.sessionDurationSecAveragePerLastHour
        && sessionDurationSecMaxPerLastHour == right.sessionDurationSecMaxPerLastHour;
}

TrafficRelayStatisticsCollector::TrafficRelayStatisticsCollector():
    m_maxSessionCountPerPeriodCalculator(kSessionStatisticsAggregationPeriod),
    m_averageSessionCountPerPeriodCalculator(kSessionStatisticsAggregationPeriod),
    m_maxSessionDurationCalculator(kSessionStatisticsAggregationPeriod),
    m_averageSessionDurationCalculator(kSessionStatisticsAggregationPeriod)
{
}

void TrafficRelayStatisticsCollector::onSessionStarted(const std::string& id)
{
    int& currentSessionCount = m_serverIdToCurrentSessionCount[id];
    ++currentSessionCount;
    m_averageSessionCountPerPeriodCalculator.add(currentSessionCount);

    ++m_currentSessionCount;

    m_maxSessionCountPerPeriodCalculator.add(currentSessionCount);
}

void TrafficRelayStatisticsCollector::onSessionStopped(
    const std::string& id,
    const std::chrono::milliseconds duration)
{
    using namespace std::chrono;

    auto it = m_serverIdToCurrentSessionCount.find(id);
    NX_ASSERT(it != m_serverIdToCurrentSessionCount.end() && it->second > 0);
    if (it != m_serverIdToCurrentSessionCount.end())
    {
        --it->second;
        m_averageSessionCountPerPeriodCalculator.add(it->second);
        if (it->second == 0)
            m_serverIdToCurrentSessionCount.erase(it);
    }

    --m_currentSessionCount;

    m_maxSessionDurationCalculator.add(duration_cast<seconds>(duration));
    m_averageSessionDurationCalculator.add(duration_cast<seconds>(duration).count());
}

RelaySessionStatistics TrafficRelayStatisticsCollector::statistics() const
{
    RelaySessionStatistics result;

    result.currentSessionCount = m_currentSessionCount;
    result.concurrentSessionToSameServerCountMaxPerHour =
        m_maxSessionCountPerPeriodCalculator.top();
    result.concurrentSessionToSameServerCountAveragePerHour =
        m_averageSessionCountPerPeriodCalculator.getAveragePerLastPeriod();
    result.sessionDurationSecMaxPerLastHour = m_maxSessionDurationCalculator.top().count();
    result.sessionDurationSecAveragePerLastHour =
        m_averageSessionDurationCalculator.getAveragePerLastPeriod();

    return result;
}

//-------------------------------------------------------------------------------------------------

TrafficRelay::~TrafficRelay()
{
    decltype(m_relaySessions) relaySessions;
    {
        QnMutexLocker lock(&m_mutex);
        m_terminated = true;
        m_relaySessions.swap(relaySessions);
    }

    for (const auto& relaySession: relaySessions)
        relaySession.channelBridge->pleaseStopSync();
}

void TrafficRelay::startRelaying(
    RelayConnectionData clientConnection,
    RelayConnectionData serverConnection)
{
    NX_VERBOSE(this, lm("Starting relaying between %1 and %2")
        .arg(clientConnection.peerId).arg(serverConnection.peerId));

    RelaySession relaySession;
    relaySession.startTime = nx::utils::monotonicTime();
    relaySession.clientPeerId = std::move(clientConnection.peerId);
    relaySession.serverPeerId = std::move(serverConnection.peerId);
    relaySession.channelBridge = network::aio::makeAsyncChannelBridge(
        std::move(clientConnection.connection),
        std::move(serverConnection.connection));
    relaySession.channelBridge->bindToAioThread(
        network::SocketGlobals::aioService().getRandomAioThread());

    QnMutexLocker lock(&m_mutex);

    m_relaySessions.push_back(std::move(relaySession));
    m_relaySessions.back().channelBridge->start(
        std::bind(&TrafficRelay::onRelaySessionFinished, this, --m_relaySessions.end()));

    m_statisticsCollector.onSessionStarted(m_relaySessions.back().serverPeerId);
}

RelaySessionStatistics TrafficRelay::statistics() const
{
    QnMutexLocker lock(&m_mutex);
    return m_statisticsCollector.statistics();
}

void TrafficRelay::onRelaySessionFinished(
    std::list<RelaySession>::iterator sessionIter)
{
    using namespace std::chrono;

    QnMutexLocker lock(&m_mutex);

    if (m_terminated)
        return;

    NX_VERBOSE(this, lm("Done relaying between %1 and %2")
        .arg(sessionIter->clientPeerId).arg(sessionIter->serverPeerId));

    m_statisticsCollector.onSessionStopped(
        sessionIter->serverPeerId,
        duration_cast<seconds>(nx::utils::monotonicTime() - sessionIter->startTime));
    m_relaySessions.erase(sessionIter);
}

//-------------------------------------------------------------------------------------------------

TrafficRelayFactory::TrafficRelayFactory():
    base_type(std::bind(&TrafficRelayFactory::defaultFactoryFunction, this))
{
}

TrafficRelayFactory& TrafficRelayFactory::instance()
{
    static TrafficRelayFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<AbstractTrafficRelay> TrafficRelayFactory::defaultFactoryFunction()
{
    return std::make_unique<TrafficRelay>();
}

} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
