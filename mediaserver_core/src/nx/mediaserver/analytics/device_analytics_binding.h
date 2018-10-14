#pragma once

#include <QtCore/QVariant>

#include <core/resource/resource_fwd.h>

#include <nx/utils/std/optional.h>

#include <nx/sdk/analytics/engine.h>
#include <nx/sdk/analytics/device_agent.h>

#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <nx/mediaserver/sdk_support/pointers.h>
#include <nx/mediaserver/resource/resource_fwd.h>

namespace nx::mediaserver::analytics {

class DeviceAnalyticsBinding
{
    using Engine = nx::sdk::analytics::Engine;
    using DeviceAgent = nx::sdk::analytics::DeviceAgent;
public:
    DeviceAnalyticsBinding(
        QnVirtualCameraResourcePtr device,
        nx::mediaserver::resource::AnalyticsEngineResourcePtr engine);

    bool startAnalytics(const QVariantMap& settings);
    void stopAnalytics();

    bool restartAnalytics(const QVariantMap& settings);

private:
    sdk_support::SharedPtr<DeviceAgent> createDeviceAgent();
    std::optional<nx::vms::api::analytics::DeviceAgentManifest> loadManifest(
        const sdk_support::SharedPtr<DeviceAgent>& deviceAgent);

    void updateDeviceWithManifest(const nx::vms::api::analytics::DeviceAgentManifest& manifest);

private:
    QnVirtualCameraResourcePtr m_device;
    nx::mediaserver::resource::AnalyticsEngineResourcePtr m_engine;

    sdk_support::SharedPtr<DeviceAgent> m_sdkDeviceAgent;

    QVariantMap m_currentSettings;
    bool m_started{false};
};

} // namespace nx::mediaserver::analytics
