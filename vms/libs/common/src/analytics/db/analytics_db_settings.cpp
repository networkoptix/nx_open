#include "analytics_db_settings.h"

#include <nx/utils/deprecated_settings.h>

namespace nx::analytics::db {

void Settings::load(const QnSettings& settings)
{
    path = settings.value("analyticsDb/path").toString();

    dbConnectionOptions.loadFromSettings(settings);
}

} // namespace nx::analytics::db
