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
    explicit ProxyAnalyticsDriver(const QnVirtualCameraResourcePtr& camera,
        QObject* parent = nullptr);


private:
    const QnVirtualCameraResourcePtr m_camera;
};

} // namespace desktop
} // namespace client
} // namespace nx
