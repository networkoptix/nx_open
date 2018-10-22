#include "analytics_engine_resource.h"

#include <nx/sdk/analytics/plugin.h>

#include <media_server/media_server_module.h>

#include <nx/mediaserver/sdk_support/utils.h>
#include <nx/mediaserver/sdk_support/pointers.h>
#include <nx/mediaserver/analytics/sdk_object_pool.h>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/engine_manifest.h>

#include <nx/utils/meta/member_detector.h>

#include <nx/analytics/descriptor_list_manager.h>

namespace nx::mediaserver::resource {

namespace analytics_sdk = nx::sdk::analytics;
namespace analytics_api = nx::vms::api::analytics;

namespace {

using PluginPtr = sdk_support::SharedPtr<analytics_sdk::Plugin>;
using EnginePtr = sdk_support::SharedPtr<analytics_sdk::Engine>;

PluginPtr getSdkPlugin(const QnUuid& pluginId, QnMediaServerModule* serverModule)
{
    if (!serverModule)
    {
        NX_ASSERT(false, "Can't access server module");
        return PluginPtr();
    }

    auto sdkObjectPool = serverModule->sdkObjectPool();
    if (!sdkObjectPool)
    {
        NX_ASSERT(false, "Can't access SDK object pool");
        return PluginPtr();
    }

    return sdkObjectPool->plugin(pluginId);
}

EnginePtr getSdkEngine(const QnUuid& engineId, QnMediaServerModule* serverModule)
{
    if (!serverModule)
    {
        NX_ASSERT(false, "Can't access server module");
        return EnginePtr();
    }

    auto sdkObjectPool = serverModule->sdkObjectPool();
    if (!sdkObjectPool)
    {
        NX_ASSERT(false, "Can't access SDK object pool");
        return EnginePtr();
    }

    return sdkObjectPool->engine(engineId);
}

} // namespace

AnalyticsEngineResource::AnalyticsEngineResource(QnMediaServerModule* serverModule):
    base_type(),
    ServerModuleAware(serverModule)
{
}

EnginePtr AnalyticsEngineResource::sdkEngine() const
{
    return getSdkEngine(getId(), serverModule());
}

CameraDiagnostics::Result AnalyticsEngineResource::initInternal()
{
    NX_DEBUG(this, lm("Initializing analytics engine resource %1 (%2)")
        .args(getName(), getId()));

    auto sdkObjectPool = sdk_support::getSdkObjectPool(serverModule());
    if (!sdkObjectPool)
        return CameraDiagnostics::InternalServerErrorResult("Can't access SDK object pool");

    auto settings = QJsonObject::fromVariantMap(settingsValues());
    auto engine = sdkObjectPool->instantiateEngine(getId(), settings);

    if (!engine)
        return CameraDiagnostics::PluginErrorResult("Can't instantiate an analytics engine");

    const auto manifest = sdk_support::manifest<analytics_api::EngineManifest>(engine);
    if (!manifest)
        return CameraDiagnostics::PluginErrorResult("Can't deserialize engine manifest");

    auto analyticsDescriptorListManager = sdk_support::getDescriptorListManager(serverModule());
    if (!analyticsDescriptorListManager)
    {
        return CameraDiagnostics::InternalServerErrorResult(
            "Can't access analytics descriptor list manager");
    }

    const auto parentPlugin = plugin();
    if (!parentPlugin)
        return CameraDiagnostics::InternalServerErrorResult("Can't access parent plugin");

    const auto pluginManifest = parentPlugin->manifest();

    analyticsDescriptorListManager->addDescriptors(
        sdk_support::descriptorsFromItemList<analytics_api::GroupDescriptor>(
            pluginManifest.id, manifest->groups));

    analyticsDescriptorListManager->addDescriptors(
        sdk_support::descriptorsFromItemList<analytics_api::EventTypeDescriptor>(
            pluginManifest.id, manifest->eventTypes));

    analyticsDescriptorListManager->addDescriptors(
        sdk_support::descriptorsFromItemList<analytics_api::ObjectTypeDescriptor>(
            pluginManifest.id, manifest->objectTypes));

    setManifest(*manifest);
    saveParams();
    return CameraDiagnostics::NoErrorResult();
}

} // nx::mediaserver::resource
