#pragma once

#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/analytics/drivers/abstract_analytics_driver.h>

namespace nx {
namespace client {
namespace desktop {

class ProxyAnalyticsDriver: public AbstractAnalyticsDriver
{
    using base_type = AbstractAnalyticsDriver;

public:
    explicit ProxyAnalyticsDriver(const QnResourcePtr& resource, QObject* parent = nullptr);


private:
    const QnResourcePtr m_resource;
};

} // namespace desktop
} // namespace client
} // namespace nx
