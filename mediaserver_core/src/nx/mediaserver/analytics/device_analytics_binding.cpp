#include "device_analytics_binding.h"

#include <plugins/plugins_ini.h>

#include <core/resource/camera_resource.h>

#include <nx/utils/log/log.h>

#include <nx/sdk/common.h>
#include <nx/sdk/analytics/engine.h>
#include <nx/sdk/analytics/plugin.h>

#include <nx/mediaserver/sdk_support/utils.h>
#include <nx/mediaserver/sdk_support/traits.h>
#include <nx/mediaserver/resource/analytics_engine_resource.h>
#include <nx/mediaserver/resource/analytics_plugin_resource.h>


namespace nx::mediaserver::analytics {

using namespace nx::vms::api::analytics;

DeviceAnalyticsBinding::DeviceAnalyticsBinding(
    QnVirtualCameraResourcePtr device,
    nx::mediaserver::resource::AnalyticsEngineResourcePtr engine)
    :
    m_device(std::move(device)),
    m_engine(std::move(engine))
{
}

bool DeviceAnalyticsBinding::startAnalytics(const QVariantMap& settings)
{
    if (!m_sdkDeviceAgent)
    {
        m_sdkDeviceAgent = createDeviceAgent();
        auto manifest = loadManifest(m_sdkDeviceAgent);
        if (!manifest)
        {
            NX_ERROR(this, lm("Cannot load device agent manifest, device %1 (%2)")
                .args(m_device->getUserDefinedName(), m_device->getId()));
            return false;
        }

        updateDeviceWithManifest(*manifest);
    }

    if (!m_sdkDeviceAgent)
        return false;

    if (m_currentSettings != settings)
    {
        const auto sdkSettings = sdk_support::toSettingsHolder(settings);
        m_sdkDeviceAgent->setSettings(sdkSettings->array(), sdkSettings->size());
        m_currentSettings = settings;
    }

    if (!m_started)
    {
        // TODO: #dmishin Pass event list.
        auto error = m_sdkDeviceAgent->startFetchingMetadata(
            /*typeList*/ nullptr,
            /*typeListSize*/ 0);
        m_started = error == nx::sdk::Error::noError;
    }

    return m_started;
}

void DeviceAnalyticsBinding::stopAnalytics()
{
    if (!m_sdkDeviceAgent)
        return;

    m_sdkDeviceAgent->stopFetchingMetadata();
}

bool DeviceAnalyticsBinding::restartAnalytics(const QVariantMap& settings)
{
    stopAnalytics();
    m_sdkDeviceAgent.reset();
    return startAnalytics(settings);
}

sdk_support::SharedPtr<DeviceAnalyticsBinding::DeviceAgent>
    DeviceAnalyticsBinding::createDeviceAgent()
{
    if (!m_device)
    {
        NX_ASSERT(false, "Device is empty");
        return nullptr;
    }

    if (!m_engine)
    {
        NX_ASSERT(false, "Engine is empty");
        return nullptr;
    }

    NX_DEBUG(
        this,
        lm("Creating DeviceAgent for device %1 (%2).")
            .args(m_device->getUserDefinedName(), m_device->getId()));

    nx::sdk::DeviceInfo deviceInfo;
    bool success = sdk_support::deviceInfoFromResource(m_device, &deviceInfo);
    if (!success)
    {
        NX_WARNING(this, lm("Cannot create device info from device %1 (%2)")
            .args(m_device->getUserDefinedName(), m_device->getId()));
        return nullptr;
    }

    NX_DEBUG(this, lm("Device info for device %1 (%2): %3")
        .args(m_device->getUserDefinedName(), m_device->getId(), deviceInfo));
    auto sdkEngine = m_engine->sdkEngine();
    if (!sdkEngine)
    {
        NX_WARNING(this, lm("Can't access SDK engine object for engine %1 (%2)")
            .args(m_engine->getName(), m_engine->getId()));
        return nullptr;
    }

    nx::sdk::Error error = nx::sdk::Error::noError;
    sdk_support::SharedPtr<DeviceAgent> deviceAgent(
        sdkEngine->obtainDeviceAgent(&deviceInfo, &error));

    if (!deviceAgent)
    {
        NX_ERROR(this, lm("Cannot obtain DeviceAgent %1 (%2), plugin returned null.")
            .args(m_device->getUserDefinedName(), m_device->getId()));
        return nullptr;
    }

    if (error != nx::sdk::Error::noError)
    {
        NX_ERROR(this, lm("Cannot obtain DeviceAgent %1 (%2), plugin returned error %3.")
            .args(m_device->getUserDefinedName(), m_device->getId()), error);
        return nullptr;
    }

    return deviceAgent;
}

std::optional<DeviceAgentManifest> DeviceAnalyticsBinding::loadManifest(
    const sdk_support::SharedPtr<DeviceAgent>& deviceAgent)
{
    if (!deviceAgent)
    {
        NX_ASSERT(false, "Invalid device agent");
        return std::nullopt;
    }

    if (!m_device)
    {
        NX_ASSERT(false, "Invalid device");
        return std::nullopt;
    }

    nx::sdk::Error error = nx::sdk::Error::noError;
    auto deleter = [deviceAgent](const char* ptr) { deviceAgent->freeManifest(ptr); };
    std::unique_ptr<const char, decltype(deleter)> deviceAgentManifest{
        deviceAgent->manifest(&error), deleter};

    if (error != nx::sdk::Error::noError)
    {
        NX_ERROR(
            this,
            lm("Can not fetch manifest for device %1 (%2), plugin returned error.")
                .args(m_device->getUserDefinedName(), m_device->getId()));

        return std::nullopt;
    }

    if (!deviceAgentManifest)
    {
        NX_ERROR(this) << lm("Received null DeviceAgent manifest for plugin, device")
            .args(m_device->getUserDefinedName(), m_device->getId());

        return std::nullopt;
    }

    if (pluginsIni().analyticsManifestOutputPath[0])
    {
        sdk_support::saveManifestToFile(
            nx::utils::log::Tag(typeid(this)),
            deviceAgentManifest.get(),
            "DeviceAgent",
            m_engine->getName(),
            lit("_device_agent"));
    }

    return sdk_support::manifest<DeviceAgentManifest>(deviceAgentManifest.get());
}

void DeviceAnalyticsBinding::updateDeviceWithManifest(
    const nx::vms::api::analytics::DeviceAgentManifest& manifest)
{
    m_device->setDeviceAgentManifest(m_engine->getId(), manifest);
}

} // namespace nx::mediaserver::analytics
