#include "settings.h"

#include <nx/utils/deprecated_settings.h>

namespace nx::clusterdb::map {

namespace {

static constexpr char kClusterId[] = "clusterDbMap/clusterId";

} // namespace

void Settings::load(const QnSettings& settings)
{
    clusterId = settings.value(kClusterId).toString().toStdString();
    synchronizationSettings.load(settings);
}

} // namespace nx::clusterdb::map
