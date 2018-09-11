#include "settings.h"

namespace nx::kvdb {

void Settings::load(const QnSettings& settings)
{
    dataSyncEngineSettings.load(settings);
}

} // namespace nx::kvdb
