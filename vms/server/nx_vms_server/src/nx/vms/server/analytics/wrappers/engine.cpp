#include "engine.h"

#include <media_server/media_server_module.h>

#include <core/resource/camera_resource.h>

#include <nx/sdk/analytics/i_engine.h>

#include <nx/vms/server/sdk_support/result_holder.h>
#include <nx/vms/server/sdk_support/utils.h>

#include <nx/vms/server/analytics/wrappers/device_agent.h>
#include <nx/vms/server/analytics/wrappers/manifest_processor.h>
#include <nx/vms/server/analytics/wrappers/settings_processor.h>

namespace nx::vms::server::analytics::wrappers {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using namespace nx::vms::api::analytics;
using namespace nx::vms::server::sdk_support;

Engine::Engine(
    QnMediaServerModule* serverModule,
    QWeakPointer<resource::AnalyticsEngineResource> engineResource,
    sdk::Ptr<sdk::analytics::IEngine> sdkEngine,
    QString libraryName)
    :
    base_type(serverModule, sdkEngine, libraryName),
    m_engineResource(engineResource)
{
}

DebugSettings Engine::makeManifestProcessorSettings() const
{
    const auto engineResource = this->engineResource();
    if (!engineResource)
        return DebugSettings();

    DebugSettings settings;
    settings.outputPath = pluginsIni().analyticsManifestOutputPath;
    settings.substitutePath = pluginsIni().analyticsManifestSubstitutePath;
    settings.logTag = nx::utils::log::Tag(typeid(this));

    return settings;
}

DebugSettings Engine::makeSettingsProcessorSettings() const
{
    const auto engineResource = this->engineResource();
    if (!engineResource)
        return DebugSettings();

    DebugSettings settings;
    settings.outputPath = pluginsIni().analyticsSettingsOutputPath;
    settings.substitutePath = pluginsIni().analyticsSettingsSubstitutePath;
    settings.logTag = nx::utils::log::Tag(typeid(this));

    return settings;
}

SdkObjectDescription Engine::sdkObjectDescription() const
{
    const auto engineResource = this->engineResource();
    if (!NX_ASSERT(engineResource))
        return SdkObjectDescription();

    const auto pluginResource = this->pluginResource();
    if (!NX_ASSERT(pluginResource))
        return SdkObjectDescription();

    return SdkObjectDescription(pluginResource, engineResource, QnVirtualCameraResourcePtr());
}

void Engine::setEngineInfo(Ptr<const IEngineInfo> engineInfo)
{
    const Ptr<IEngine> engine = sdkObject();
    if (!NX_ASSERT(engine))
        return;

    engine->setEngineInfo(engineInfo.get());
}

bool Engine::isCompatible(QnVirtualCameraResourcePtr device) const
{
    const Ptr<IEngine> engine = sdkObject();
    if (!NX_ASSERT(engine))
        return false;

    const auto deviceInfo = sdk_support::deviceInfoFromResource(device);
    if (!deviceInfo)
    {
        NX_WARNING(this, lm("Cannot create device info from device %1 (%2)")
            .args(device->getUserDefinedName(), device->getId()));
        return false;
    }

    NX_DEBUG(this, lm("Device info for device %1 (%2): %3")
        .args(device->getUserDefinedName(), device->getId(), deviceInfo));

    const ResultHolder<bool> result = engine->isCompatible(deviceInfo.get());
    if (!result.isOk())
    {
        return handleError(
            SdkMethod::isCompatible,
            Error::fromResultHolder(result),
            /*returnValue*/ false);
    }

    return result.value();
}

std::shared_ptr<DeviceAgent> Engine::obtainDeviceAgent(QnVirtualCameraResourcePtr device)
{
    const Ptr<IEngine> engine = sdkObject();
    if (!NX_ASSERT(engine))
        return nullptr;

    const auto deviceInfo = sdk_support::deviceInfoFromResource(device);
    if (!deviceInfo)
    {
        NX_WARNING(this, lm("Cannot create device info from device %1 (%2)")
            .args(device->getUserDefinedName(), device->getId()));
        return nullptr;
    }

    NX_DEBUG(this, lm("Device info for device %1 (%2): %3")
        .args(device->getUserDefinedName(), device->getId(), deviceInfo));

    const ResultHolder<IDeviceAgent*> result = engine->obtainDeviceAgent(deviceInfo.get());
    if (!result.isOk())
    {
        return handleError(
            SdkMethod::obtainDeviceAgent,
            Error::fromResultHolder(result),
            /*returnValue*/ nullptr);
    }

    return std::make_shared<wrappers::DeviceAgent>(
        serverModule(),
        engineResource(),
        device,
        result.value(),
        libName());
}

bool Engine::executeAction(Ptr<IAction> action)
{
    const Ptr<IEngine> engine = sdkObject();
    if (!NX_ASSERT(engine))
        return false;

    const ResultHolder<void> result = engine->executeAction(action.get());
    if (!result.isOk())
    {
        return handleError(
            SdkMethod::executeAction,
            Error::fromResultHolder(result),
            /*returnValue*/ false);
    }

    return true;
}

void Engine::setHandler(sdk::Ptr<sdk::analytics::IEngine::IHandler> handler)
{
    const Ptr<IEngine> engine = sdkObject();
    if (!NX_ASSERT(engine))
        return;

    engine->setHandler(handler.get());
}

resource::AnalyticsEngineResourcePtr Engine::engineResource() const
{
    return m_engineResource.toStrongRef();
}

resource::AnalyticsPluginResourcePtr Engine::pluginResource() const
{
    const auto engineResource = this->engineResource();
    if (!NX_ASSERT(engineResource))
        return nullptr;

    return engineResource->plugin().dynamicCast<resource::AnalyticsPluginResource>();
}

} // namespace nx::vms::server::analytics::wrappers
