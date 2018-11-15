#include "p2p_sync_settings.h"

#include <nx/utils/deprecated_settings.h>

namespace nx::clusterdb::engine {

namespace {

static const QLatin1String kMaxConcurrentConnectionsFromSystem(
    "p2pDb/maxConcurrentConnectionsFromSystem");
constexpr const unsigned int kMaxConcurrentConnectionsFromSystemDefault = 2;

} // namespace

SynchronizationSettings::SynchronizationSettings():
    maxConcurrentConnectionsFromSystem(
        kMaxConcurrentConnectionsFromSystemDefault)
{
}

void SynchronizationSettings::load(const QnSettings& settings)
{
    maxConcurrentConnectionsFromSystem = settings.value(
        kMaxConcurrentConnectionsFromSystem,
        kMaxConcurrentConnectionsFromSystemDefault).toInt();
}

} // namespace nx::clusterdb::engine
