#include "p2p_sync_settings.h"

#include <nx/utils/deprecated_settings.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/uuid.h>

namespace nx::clusterdb::engine {

namespace {

static constexpr char kNodeId[] = "nodeId";

static constexpr char kClusterId[] = "clusterId";

static constexpr char kMaxConcurrentConnectionsFromSystem[] =
    "maxConcurrentConnectionsFromSystem";
static constexpr int kMaxConcurrentConnectionsFromSystemDefault = 2;

static constexpr char kNodeConnectRetryTimeout[] = "nodeConnectRetryTimeout";
static constexpr std::chrono::seconds kDefaultNodeConnectRetryTimeout = std::chrono::seconds(7);

} // namespace

SynchronizationSettings::SynchronizationSettings():
    nodeId(QnUuid::createUuid().toStdString()),
    maxConcurrentConnectionsFromSystem(
        kMaxConcurrentConnectionsFromSystemDefault),
    nodeConnectRetryTimeout(kDefaultNodeConnectRetryTimeout)
{
}

void SynchronizationSettings::load(const QnSettings& settings, std::string groupName)
{
    QString settingsTemplate = groupName.empty() ? "%1%2" : "%1/%2";

    clusterId = settings.value(
        lm(settingsTemplate).arg(groupName).arg(kClusterId),
        clusterId.c_str()).toString().toStdString();

    nodeId = settings.value(
        lm(settingsTemplate).arg(groupName).arg(kNodeId),
        nodeId.c_str()).toString().toStdString();

    if (clusterId.empty())
    {
        maxConcurrentConnectionsFromSystem = settings.value(
            lm(settingsTemplate).arg(groupName).arg(kMaxConcurrentConnectionsFromSystem),
            kMaxConcurrentConnectionsFromSystemDefault).toInt();
    }
    else
    {
        maxConcurrentConnectionsFromSystem = std::numeric_limits<int>::max();
    }

    nodeConnectRetryTimeout = nx::utils::parseTimerDuration(settings.value(
        lm(settingsTemplate).arg(groupName).arg(kNodeConnectRetryTimeout)).toString(),
        kDefaultNodeConnectRetryTimeout);

    discovery.load(settings, groupName + "/" + "discovery");
}

} // namespace nx::clusterdb::engine
