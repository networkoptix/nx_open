#include "p2p_sync_settings.h"

#include <nx/utils/deprecated_settings.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/uuid.h>

namespace nx::clusterdb::engine {

namespace {

static constexpr char kNodeId[] = "nodeId";

static constexpr char kClusterId[] = "clusterId";

static constexpr char kMaxConcurrentConnectionsFromCluster[] =
    "maxConcurrentConnectionsFromCluster";
static constexpr int kMaxConcurrentConnectionsFromClusterDefault = 2;

static constexpr char kNodeConnectRetryTimeout[] = "nodeConnectRetryTimeout";
static constexpr std::chrono::seconds kDefaultNodeConnectRetryTimeout = std::chrono::seconds(7);

static constexpr char kGroupCommandsUnderDbTransaction[] = "groupCommandsUnderDbTransaction";
static constexpr bool kDefaultGroupCommandsUnderDbTransaction = false;

} // namespace

SynchronizationSettings::SynchronizationSettings():
    nodeId(QnUuid::createUuid().toStdString()),
    maxConcurrentConnectionsFromCluster(
        kMaxConcurrentConnectionsFromClusterDefault),
    nodeConnectRetryTimeout(kDefaultNodeConnectRetryTimeout)
{
}

void SynchronizationSettings::load(const QnSettings& settings, std::string groupName)
{
    while (!groupName.empty() && groupName.back() == '/')
        groupName.pop_back();

    QString settingsTemplate = groupName.empty() ? "%1%2" : "%1/%2";

    clusterId = settings.value(
        lm(settingsTemplate).arg(groupName).arg(kClusterId),
        clusterId.c_str()).toString().toStdString();

    nodeId = settings.value(
        lm(settingsTemplate).arg(groupName).arg(kNodeId),
        nodeId.c_str()).toString().toStdString();

    if (clusterId.empty())
    {
        maxConcurrentConnectionsFromCluster = settings.value(
            lm(settingsTemplate).arg(groupName).arg(kMaxConcurrentConnectionsFromCluster),
            kMaxConcurrentConnectionsFromClusterDefault).toInt();
    }
    else
    {
        maxConcurrentConnectionsFromCluster = std::numeric_limits<int>::max();
    }

    nodeConnectRetryTimeout = nx::utils::parseTimerDuration(settings.value(
        lm(settingsTemplate).arg(groupName).arg(kNodeConnectRetryTimeout)).toString(),
        kDefaultNodeConnectRetryTimeout);

    discovery.load(settings, groupName.empty() ? "discovery" : groupName + "/discovery");

    groupCommandsUnderDbTransaction = settings.value(
        lm(settingsTemplate).args(groupName, kGroupCommandsUnderDbTransaction),
        kDefaultGroupCommandsUnderDbTransaction ? "true" : "false").toString() == "true";
}

} // namespace nx::clusterdb::engine
