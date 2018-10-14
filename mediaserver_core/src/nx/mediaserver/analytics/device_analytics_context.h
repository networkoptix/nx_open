#pragma once

#include <map>

#include <utils/common/connective.h>
#include <core/resource/resource_fwd.h>

#include <nx/mediaserver/server_module_aware.h>
#include <nx/mediaserver/sdk_support/pointers.h>
#include <nx/sdk/analytics/device_agent.h>

namespace nx::mediaserver::analytics {

class DeviceAnalyticsBinding;

class DeviceAnalyticsContext:
    public Connective<QObject>,
    public nx::mediaserver::ServerModuleAware
{
    using base_type = nx::mediaserver::ServerModuleAware;
public:
    DeviceAnalyticsContext(
        QnMediaServerModule* severModule,
        const QnVirtualCameraResourcePtr& device);

    void setEnabledAnalyticsEngines(
        const nx::vms::common::AnalyticsEngineResourceList& engines);

    void addEngine(const nx::vms::common::AnalyticsEngineResourcePtr& engine);
    void removeEngine(const nx::vms::common::AnalyticsEngineResourcePtr& engine);

private:
    void at_deviceUpdated(const QnResourcePtr& device);
    void at_devicePropertyChanged(const QnResourcePtr& device, const QString& propertyName);

private:
    void subscribeToDeviceChanges();

    bool isEngineAlreadyBound(const QnUuid& engineId) const;
    bool isEngineAlreadyBound(const nx::vms::common::AnalyticsEngineResourcePtr& engine) const;

private:
    QnVirtualCameraResourcePtr m_device;
    std::map<QnUuid, std::shared_ptr<DeviceAnalyticsBinding>> m_bindings;
};

} // namespace nx::mediaserver::analytics
