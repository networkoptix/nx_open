#pragma once

#include <optional>

#include <core/resource/resource_fwd.h>

#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/server/analytics/wrappers/types.h>
#include <nx/vms/server/analytics/wrappers/base_engine.h>
#include <nx/vms/server/sdk_support/error.h>

#include <nx/sdk/result.h>
#include <nx/sdk/i_string.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/analytics/i_engine.h>
#include <nx/sdk/analytics/i_consuming_device_agent.h>

#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <nx/vms/event/events/events_fwd.h>
#include <nx/vms/event/events/plugin_diagnostic_event.h>

namespace nx::vms::server::analytics::wrappers {

class DeviceAgent;

class Engine: public BaseEngine<sdk::analytics::IEngine, api::analytics::EngineManifest>
{
    using base_type = BaseEngine<sdk::analytics::IEngine, api::analytics::EngineManifest>;
public:
    Engine(
        QnMediaServerModule* serverModule,
        const QWeakPointer<resource::AnalyticsEngineResource> engineResource,
        const sdk::Ptr<sdk::analytics::IEngine> sdkEngine,
        QString libraryName);

    void setEngineInfo(sdk::Ptr<const sdk::analytics::IEngineInfo> engineInfo);

    bool isCompatible(QnVirtualCameraResourcePtr device) const;

    std::shared_ptr<DeviceAgent> obtainDeviceAgent(QnVirtualCameraResourcePtr deviceInfo);

    bool executeAction(sdk::Ptr<sdk::analytics::IAction> action);

    void setHandler(sdk::Ptr<sdk::analytics::IEngine::IHandler> handler);

protected:
    virtual DebugSettings makeProcessorSettings() const override;

    virtual SdkObjectDescription sdkObjectDescription() const override;

private:
    resource::AnalyticsEngineResourcePtr engineResource() const;
    resource::AnalyticsPluginResourcePtr pluginResource() const;

private:
    QWeakPointer<resource::AnalyticsEngineResource> m_engineResource;
};

} // namespace nx::vms::server::analytics::wrappers
