#include "settings.h"

#include <nx/utils/timer_manager.h>

namespace nx {
namespace cloud {
namespace relaying {

namespace {

//-------------------------------------------------------------------------------------------------
// Settings

static const QLatin1String kRecommendedPreemptiveConnectionCount(
    "listeningPeer/recommendedPreemptiveConnectionCount");
constexpr int kDefaultRecommendedPreemptiveConnectionCount = 7;

static const QLatin1String kMaxPreemptiveConnectionCount(
    "listeningPeer/maxPreemptiveConnectionCount");
constexpr int kDefaultMaxPreemptiveConnectionCount =
    kDefaultRecommendedPreemptiveConnectionCount * 2;

static const QLatin1String kDisconnectedPeerTimeout("listeningPeer/disconnectedPeerTimeout");
static std::chrono::milliseconds kDefaultDisconnectedPeerTimeout = std::chrono::seconds(15);

static const QLatin1String kTakeIdleConnectionTimeout("listeningPeer/takeIdleConnectionTimeout");
static std::chrono::milliseconds kDefaultTakeIdleConnectionTimeout = std::chrono::seconds(5);

static const QLatin1String kInternalTimerPeriod("listeningPeer/internalTimerPeriod");
static std::chrono::milliseconds kDefaultInternalTimerPeriod = std::chrono::seconds(1);

static const QLatin1String kInactivityPeriodBeforeFirstProbe(
    "listeningPeer/tcpInactivityPeriodBeforeFirstProbe");
static const std::chrono::seconds kDefaultInactivityPeriodBeforeFirstProbe(30);

static const QLatin1String kProbeSendPeriod("listeningPeer/tcpProbeSendPeriod");
static const std::chrono::seconds kDefaultProbeSendPeriod(30);

static const QLatin1String kProbeCount("listeningPeer/tcpProbeCount");
static const int kDefaultProbeCount(2);

} // namespace

//-------------------------------------------------------------------------------------------------

Settings::Settings():
    recommendedPreemptiveConnectionCount(kDefaultRecommendedPreemptiveConnectionCount),
    maxPreemptiveConnectionCount(kDefaultMaxPreemptiveConnectionCount),
    disconnectedPeerTimeout(kDefaultDisconnectedPeerTimeout),
    takeIdleConnectionTimeout(kDefaultTakeIdleConnectionTimeout),
    internalTimerPeriod(kDefaultInternalTimerPeriod),
    tcpKeepAlive()
{
}

void Settings::load(const QnSettings& settings)
{
    using namespace std::chrono;

    recommendedPreemptiveConnectionCount = settings.value(
        kRecommendedPreemptiveConnectionCount,
        kDefaultRecommendedPreemptiveConnectionCount).toInt();

    maxPreemptiveConnectionCount = settings.value(
        kMaxPreemptiveConnectionCount,
        recommendedPreemptiveConnectionCount * 2).toInt();

    disconnectedPeerTimeout =
        nx::utils::parseTimerDuration(
            settings.value(kDisconnectedPeerTimeout).toString(),
            kDefaultDisconnectedPeerTimeout);

    takeIdleConnectionTimeout =
        nx::utils::parseTimerDuration(
            settings.value(kTakeIdleConnectionTimeout).toString(),
            kDefaultTakeIdleConnectionTimeout);

    internalTimerPeriod =
        nx::utils::parseTimerDuration(
            settings.value(kInternalTimerPeriod).toString(),
            kDefaultInternalTimerPeriod);

    tcpKeepAlive.inactivityPeriodBeforeFirstProbe = duration_cast<seconds>(
        nx::utils::parseTimerDuration(
            settings.value(kInactivityPeriodBeforeFirstProbe).toString(),
            kDefaultInactivityPeriodBeforeFirstProbe));

    tcpKeepAlive.probeSendPeriod = duration_cast<seconds>(
        nx::utils::parseTimerDuration(
            settings.value(kProbeSendPeriod).toString(),
            kDefaultProbeSendPeriod));

    tcpKeepAlive.probeCount = settings.value(
        kProbeCount,
        kDefaultProbeCount).toInt();
}

} // namespace relaying
} // namespace cloud
} // namespace nx
