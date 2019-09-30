#include "settings.h"

#include <nx/utils/deprecated_settings.h>

namespace nx::clusterdb::map {

namespace {

static constexpr char kEnableCache[] = "enableCache";
static constexpr bool kDefaultEnableCache = false;

} // namespace

void Settings::load(const QnSettings& settings, std::string groupName)
{
    const char* settingsTemplate = groupName.empty() ? "%1%2" : "%1/%2";
    enableCache = settings.value(
        lm(settingsTemplate).args(groupName, kEnableCache), kDefaultEnableCache).toBool();
    synchronizationSettings.load(settings, groupName);
}

} // namespace nx::clusterdb::map
