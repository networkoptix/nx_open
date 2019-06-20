#include "settings.h"

#include <nx/utils/deprecated_settings.h>

namespace nx::clusterdb::map {

void Settings::load(const QnSettings& settings, std::string groupName)
{
    synchronizationSettings.load(settings, groupName);
}

} // namespace nx::clusterdb::map
