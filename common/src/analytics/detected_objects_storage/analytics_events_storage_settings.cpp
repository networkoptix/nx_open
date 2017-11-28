#include "analytics_events_storage_settings.h"

namespace nx {
namespace analytics {
namespace storage {

void Settings::load(const QnSettings& settings)
{
    dbConnectionOptions.loadFromSettings(settings);
}

} // namespace storage
} // namespace analytics
} // namespace nx
