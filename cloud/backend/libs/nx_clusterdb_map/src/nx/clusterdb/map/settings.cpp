#include "settings.h"

#include <nx/utils/deprecated_settings.h>

namespace nx::clusterdb::map {

namespace {

static constexpr char kClusterId[] = "clusterDbMap/clusterId";

} // namespace

void Settings::load(const QnSettings& settings)
{
    clusterId = settings.value(kClusterId).toString().toStdString();

    std::string message = clusterId.empty()
        ? "clusterId is empty, cluster db map loaded in standalone mode"
        : std::string("cluster db map loaded with clusterId: ") + clusterId;

    NX_DEBUG(this, message);

    synchronizationSettings.load(settings);
}

} // namespace nx::clusterdb::map
