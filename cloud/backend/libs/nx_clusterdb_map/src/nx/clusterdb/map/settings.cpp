#include "settings.h"

#include <nx/utils/deprecated_settings.h>

namespace nx::clusterdb::map {

void Settings::load(const QnSettings& settings)
{
    synchronizationSettings.load(settings);
}

} // namespace nx::clusterdb::map
