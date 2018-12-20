#include "settings.h"

namespace nx::clusterdb::map {

void Settings::load(const QnSettings& settings)
{
    dataSyncEngineSettings.load(settings);
}

} // namespace nx::clusterdb::map
