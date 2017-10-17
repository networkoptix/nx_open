#include "analytics_drivers_factory.h"

#include <core/resource/camera_resource.h>

#include <ini.h>

#include <nx/client/desktop/analytics/drivers/demo_analytics_driver.h>
#include <nx/client/desktop/analytics/drivers/proxy_analytics_driver.h>

namespace nx {
namespace client {
namespace desktop {

AbstractAnalyticsDriverPtr AnalyticsDriversFactory::createAnalyticsDriver(
    const QnResourcePtr& resource)
{
    if (!ini().enableAnalytics)
        return AbstractAnalyticsDriverPtr();

    if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
        return AbstractAnalyticsDriverPtr(new ProxyAnalyticsDriver(camera));

    if (ini().externalMetadata)
    {

    }

    if (ini().demoAnalyticsDriver)
        return AbstractAnalyticsDriverPtr(new DemoAnalyticsDriver());

    return AbstractAnalyticsDriverPtr();
}

} // namespace desktop
} // namespace client
} // namespace nx