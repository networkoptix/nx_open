#include "p2p_sync_settings.h"

#include <nx/utils/deprecated_settings.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/uuid.h>

namespace nx::clusterdb::engine {

namespace {

static constexpr char kNodeId[] = "p2pDb/nodeId";

static constexpr char kClusterId[] = "p2pDb/clusterId";

static constexpr char kMaxConcurrentConnectionsFromSystem[] =
    "p2pDb/maxConcurrentConnectionsFromSystem";
static constexpr int kMaxConcurrentConnectionsFromSystemDefault = 2;

static constexpr char kNodeConnectRetryTimeout[] = "p2pDb/nodeConnectRetryTimeout";
static constexpr std::chrono::seconds kDefaultNodeConnectRetryTimeout = std::chrono::seconds(7);

} // namespace

SynchronizationSettings::SynchronizationSettings():
    nodeId(QnUuid::createUuid().toStdString()),
    maxConcurrentConnectionsFromSystem(
        kMaxConcurrentConnectionsFromSystemDefault),
    nodeConnectRetryTimeout(kDefaultNodeConnectRetryTimeout)
{
}

void SynchronizationSettings::load(const QnSettings& settings)
{
    clusterId = settings.value(kClusterId, clusterId.c_str()).toString().toStdString();

    nodeId = settings.value(kNodeId, nodeId.c_str()).toString().toStdString();

    if (clusterId.empty())
    {
        maxConcurrentConnectionsFromSystem = settings.value(
            kMaxConcurrentConnectionsFromSystem,
            kMaxConcurrentConnectionsFromSystemDefault).toInt();
    }
    else
    {
        maxConcurrentConnectionsFromSystem = std::numeric_limits<int>::max();
    }

    nodeConnectRetryTimeout = nx::utils::parseTimerDuration(
        settings.value(kNodeConnectRetryTimeout).toString(),
        kDefaultNodeConnectRetryTimeout);

    discovery.load(settings);
}

} // namespace nx::clusterdb::engine
