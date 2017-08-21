#include "discovery_settings.h"

#include <nx/utils/settings.h>
#include <nx/utils/timer_manager.h>

namespace nx {
namespace cloud {
namespace discovery {
namespace conf {

namespace {

static const QLatin1String kKeepAliveTimeout("discovery/keepAliveTimeout");
static const std::chrono::seconds kDefaultKeepAliveTimeout = std::chrono::seconds(15);

} // namespace

Discovery::Discovery():
    keepAliveTimeout(kDefaultKeepAliveTimeout)
{
}

void Discovery::load(const QnSettings& settings)
{
    keepAliveTimeout =
        nx::utils::parseTimerDuration(
            settings.value(kKeepAliveTimeout).toString(),
            kDefaultKeepAliveTimeout);
}

} // namespace conf
} // namespace nx
} // namespace cloud
} // namespace discovery
