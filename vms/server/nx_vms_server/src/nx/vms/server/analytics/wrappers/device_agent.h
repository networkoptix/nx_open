#pragma once

#include <optional>

#include <core/resource/resource_fwd.h>

#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/server/analytics/wrappers/types.h>
#include <nx/vms/server/analytics/wrappers/base_engine.h>
#include <nx/vms/server/sdk_support/error.h>
#include <nx/vms/server/sdk_support/types.h>

#include <nx/sdk/result.h>
#include <nx/sdk/i_string.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/i_consuming_device_agent.h>

#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <nx/vms/event/events/events_fwd.h>
#include <nx/vms/event/events/plugin_diagnostic_event.h>

namespace nx::vms::server::analytics::wrappers {

class DeviceAgent:
    public BaseEngine<sdk::analytics::IDeviceAgent, api::analytics::DeviceAgentManifest>
{
    using base_type =
        BaseEngine<sdk::analytics::IDeviceAgent, api::analytics::DeviceAgentManifest>;
public:
    DeviceAgent(
        QnMediaServerModule* serverModule,
        QWeakPointer<resource::AnalyticsEngineResource> engine,
        QWeakPointer<QnVirtualCameraResource> device,
        sdk::Ptr<sdk::analytics::IDeviceAgent> sdkDeviceAgent,
        QString libraryName);

    void setHandler(sdk::Ptr<sdk::analytics::IDeviceAgent::IHandler> handler);
    bool setNeededMetadataTypes(const sdk_support::MetadataTypes& metadataTypes);
    bool pushDataPacket(sdk::Ptr<sdk::analytics::IDataPacket> data);

    bool isConsumer() const { return m_consumingDeviceAgent; }

protected:
    virtual DebugSettings makeManifestProcessorSettings() const override;

    virtual DebugSettings makeSettingsProcessorSettings() const override;

    virtual SdkObjectDescription sdkObjectDescription() const override;

private:
    resource::AnalyticsEngineResourcePtr engineResource() const;

    resource::AnalyticsPluginResourcePtr pluginResource() const;

    QnVirtualCameraResourcePtr device() const;

private:
    QWeakPointer<resource::AnalyticsEngineResource> m_engineResource;
    QWeakPointer<QnVirtualCameraResource> m_device;

    sdk::Ptr<sdk::analytics::IConsumingDeviceAgent> m_consumingDeviceAgent;
};

} // namespace nx::vms::server::analytics::wrappers
