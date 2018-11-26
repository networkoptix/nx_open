#include "analytics_plugin_resource.h"

#include <nx/sdk/analytics/plugin.h>

#include <media_server/media_server_module.h>

#include <nx/mediaserver/sdk_support/utils.h>
#include <nx/mediaserver/sdk_support/pointers.h>

#include <nx/mediaserver/analytics/sdk_object_factory.h>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/analytics/descriptor_list_manager.h>

namespace nx::mediaserver::resource {

namespace {

nx::vms::api::analytics::PluginDescriptor descriptorFromManifest(
    const nx::vms::api::analytics::PluginManifest& manifest)
{
    nx::vms::api::analytics::PluginDescriptor descriptor;
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

void AnalyticsPluginResource::setSdkPlugin(
    sdk_support::SharedPtr<nx::sdk::analytics::Plugin> plugin)
{
    m_sdkPlugin = std::move(plugin);
}

sdk_support::SharedPtr<nx::sdk::analytics::Plugin> AnalyticsPluginResource::sdkPlugin() const
{
    return m_sdkPlugin;
}

std::unique_ptr<sdk_support::AbstractManifestLogger> AnalyticsPluginResource::makeLogger() const
{
    const QString messageTemplate(
        "Error occurred while fetching Engine manifest for engine: {:engine}: {:error}");

    return std::make_unique<sdk_support::ManifestLogger>(
        typeid(this), //< Using the same tag for all instances.
        messageTemplate,
        toSharedPointer(this));
}

CameraDiagnostics::Result AnalyticsPluginResource::initInternal()
{
    NX_DEBUG(this, "Initializing analytics plugin resource %1 (%2)", getName(), getId());

    if (!m_sdkPlugin)
        return CameraDiagnostics::InternalServerErrorResult("SDK plugin object is not set");

    const auto manifest = sdk_support::manifest<nx::vms::api::analytics::PluginManifest>(
        m_sdkPlugin,
        makeLogger());

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
    saveProperties();

    return CameraDiagnostics::NoErrorResult();
}

} // namespace nx::mediaserver::resource
