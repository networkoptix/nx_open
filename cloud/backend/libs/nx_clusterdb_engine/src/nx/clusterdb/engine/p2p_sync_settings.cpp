#include "p2p_sync_settings.h"

#include <nx/utils/deprecated_settings.h>
#include <nx/utils/uuid.h>

namespace nx::clusterdb::engine {

namespace {

static constexpr char kNodeId[] = "p2pDb/nodeId";

static constexpr char kMaxConcurrentConnectionsFromSystem[] =
    "p2pDb/maxConcurrentConnectionsFromSystem";
static constexpr int kMaxConcurrentConnectionsFromSystemDefault = 2;

} // namespace

SynchronizationSettings::SynchronizationSettings():
    nodeId(QnUuid::createUuid().toStdString()),
    maxConcurrentConnectionsFromSystem(
        kMaxConcurrentConnectionsFromSystemDefault)
{
}

void SynchronizationSettings::load(const QnSettings& settings)
{
    nodeId = settings.value(kNodeId, nodeId.c_str()).toString().toStdString();

    maxConcurrentConnectionsFromSystem = settings.value(
        kMaxConcurrentConnectionsFromSystem,
        kMaxConcurrentConnectionsFromSystemDefault).toInt();
}

} // namespace nx::clusterdb::engine
