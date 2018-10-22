#include "analytics_plugin_resource.h"

#include <nx/sdk/analytics/plugin.h>

#include <media_server/media_server_module.h>

#include <nx/mediaserver/sdk_support/utils.h>
#include <nx/mediaserver/sdk_support/pointers.h>

#include <nx/mediaserver/analytics/sdk_object_pool.h>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/analytics/descriptor_list_manager.h>

namespace nx::mediaserver::resource {

namespace analytics_api = nx::vms::api::analytics;
namespace analytics_sdk = nx::sdk::analytics;

namespace {

analytics_api::PluginDescriptor descriptorFromManifest(
    const analytics_api::PluginManifest& manifest)
{
    analytics_api::PluginDescriptor descriptor;
    descriptor.id = manifest.id;
    descriptor.name = manifest.name;

    return descriptor;
}

} // namespace

AnalyticsPluginResource::AnalyticsPluginResource(QnMediaServerModule* serverModule):
    base_type(),
    ServerModuleAware(serverModule)
{
}

CameraDiagnostics::Result AnalyticsPluginResource::initInternal()
{
    NX_DEBUG(this, lm("Initializing analytics plugin resource %1 (%2)")
        .args(getName(), getId()));

    auto sdkObjectPool = sdk_support::getSdkObjectPool(serverModule());
    if (!sdkObjectPool)
        return CameraDiagnostics::InternalServerErrorResult("Can't access SDK object pool");

    auto sdkPlugin = sdkObjectPool->plugin(getId());
    if (!sdkPlugin)
        return CameraDiagnostics::PluginErrorResult("Can't find plugin");

    const auto manifest = sdk_support::manifest<analytics_api::PluginManifest>(sdkPlugin);
    if (!manifest)
        return CameraDiagnostics::PluginErrorResult("Can't deserialize engine manifest");

    auto analyticsDescriptorListManager = sdk_support::getDescriptorListManager(serverModule());
    if (!analyticsDescriptorListManager)
    {
        return CameraDiagnostics::InternalServerErrorResult(
            "Can't access analytics descriptor list manager");
    }

    analyticsDescriptorListManager->addDescriptor(descriptorFromManifest(*manifest));
    setManifest(*manifest);
    saveParams();

    return CameraDiagnostics::NoErrorResult();
}

} // namespace nx::mediaserver::resource
