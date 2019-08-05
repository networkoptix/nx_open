#include "device_agent.h"

#include <media_server/media_server_module.h>
#include <plugins/vms_server_plugins_ini.h>
#include <core/resource/camera_resource.h>

#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log_level.h>
#include <nx/utils/log/log_main.h>
#include <nx/fusion/model_functions.h>

#include <nx/sdk/analytics/i_engine.h>

#include <nx/vms/server/sdk_support/utils.h>
#include <nx/vms/server/sdk_support/to_string.h>
#include <nx/vms/server/sdk_support/result_holder.h>
#include <nx/vms/server/event/event_connector.h>
#include <nx/vms/server/interactive_settings/json_engine.h>
#include <nx/vms/server/analytics/wrappers/manifest_processor.h>
#include <nx/vms/server/analytics/wrappers/settings_processor.h>


namespace nx::vms::server::analytics::wrappers {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using namespace nx::vms::api::analytics;
using namespace nx::vms::server::sdk_support;
using namespace nx::vms::event;

//-------------------------------------------------------------------------------------------------

DeviceAgent::DeviceAgent(
    QnMediaServerModule* serverModule,
    QWeakPointer<resource::AnalyticsEngineResource> engineResource,
    QWeakPointer<QnVirtualCameraResource> device,
    sdk::Ptr<sdk::analytics::IDeviceAgent> sdkDeviceAgent,
    QString libraryName)
    :
    base_type(serverModule, sdkDeviceAgent, libraryName),
    m_engineResource(engineResource),
    m_device(device),
    m_consumingDeviceAgent(
        sdk::queryInterfacePtr<sdk::analytics::IConsumingDeviceAgent>(sdkDeviceAgent))
{
}

DebugSettings DeviceAgent::makeProcessorSettings() const
{
    DebugSettings settings;
    settings.outputPath = pluginsIni().analyticsManifestOutputPath;
    settings.substitutePath = pluginsIni().analyticsManifestSubstitutePath;
    settings.logTag = nx::utils::log::Tag(typeid(this));

    return settings;
}

SdkObjectDescription DeviceAgent::sdkObjectDescription() const
{
    const auto engineResource = this->engineResource();
    if (!NX_ASSERT(engineResource))
        return SdkObjectDescription();

    const auto pluginResource = this->pluginResource();
    if (!NX_ASSERT(pluginResource))
        return SdkObjectDescription();

    const auto device = this->device();
    if (!NX_ASSERT(device))
        return SdkObjectDescription();

    return SdkObjectDescription(pluginResource, engineResource, device);
}

void DeviceAgent::setHandler(Ptr<IDeviceAgent::IHandler> handler)
{
    Ptr<IDeviceAgent> sdkDeviceAgent = sdkObject();
    if (!NX_ASSERT(sdkDeviceAgent))
        return;

    sdkDeviceAgent->setHandler(handler.get());
}

bool DeviceAgent::setNeededMetadataTypes(const MetadataTypes& metadataTypes)
{
    Ptr<IDeviceAgent> sdkDeviceAgent = sdkObject();
    if (!NX_ASSERT(sdkDeviceAgent))
        return false;

    const ResultHolder<void> result =
        sdkDeviceAgent->setNeededMetadataTypes(toSdkMetadataTypes(metadataTypes).get());

    if (!result.isOk())
    {
        return handleError(
            SdkMethod::setNeededMetadataTypes,
            Error::fromResultHolder(result),
            /*returnValue*/ false);
    }

    return true;
}

bool DeviceAgent::pushDataPacket(Ptr<IDataPacket> data)
{
    if (!NX_ASSERT(m_consumingDeviceAgent))
        return false;

    const ResultHolder<void> result = m_consumingDeviceAgent->pushDataPacket(data.get());
    if (!result.isOk())
    {
        return handleError(
            SdkMethod::pushDataPacket,
            Error::fromResultHolder(result),
            /*returnValue*/ false);
    }

    return true;
}

resource::AnalyticsEngineResourcePtr DeviceAgent::engineResource() const
{
    return m_engineResource.toStrongRef();
}

resource::AnalyticsPluginResourcePtr DeviceAgent::pluginResource() const
{
    const auto engineResource = this->engineResource();
    if (!NX_ASSERT(engineResource))
        return nullptr;

    return engineResource->plugin().dynamicCast<resource::AnalyticsPluginResource>();
}

QnVirtualCameraResourcePtr DeviceAgent::device() const
{
    return m_device;
}

} // namespace nx::vms::server::analytics::wrappers
