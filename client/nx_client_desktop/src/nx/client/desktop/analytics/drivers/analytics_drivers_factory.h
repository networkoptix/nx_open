#pragma once

#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/analytics/analytics_fwd.h>

namespace nx {
namespace client {
namespace desktop {

class AnalyticsDriversFactory
{
public:
    static AbstractAnalyticsDriverPtr createAnalyticsDriver(const QnResourcePtr& resource);
    static bool supportsAnalytics(const QnResourcePtr& resource);
};

} // namespace desktop
} // namespace client
} // namespace nx