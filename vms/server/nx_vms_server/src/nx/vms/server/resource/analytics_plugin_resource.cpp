#include "analytics_plugin_resource.h"

#include <media_server/media_server_module.h>

#include <nx/vms/server/sdk_support/utils.h>

#include <nx/vms/server/analytics/sdk_object_factory.h>
#include <nx/vms/server/analytics/wrappers/plugin.h>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/analytics/descriptor_manager.h>

namespace nx::vms::server::resource {

AnalyticsPluginResource::AnalyticsPluginResource(QnMediaServerModule* serverModule):
    base_type(),
    ServerModuleAware(serverModule)
{
}

void AnalyticsPluginResource::setSdkPlugin(analytics::wrappers::PluginPtr plugin)
{
    m_sdkPlugin = std::move(plugin);
}

analytics::wrappers::PluginPtr AnalyticsPluginResource::sdkPlugin() const
{
    return m_sdkPlugin;
}

CameraDiagnostics::Result AnalyticsPluginResource::initInternal()
{
    NX_DEBUG(this, "Initializing Analytics Plugin resource %1 (%2)", getName(), getId());

    if (!m_sdkPlugin)
        return CameraDiagnostics::InternalServerErrorResult("SDK Plugin object is not set");

    const auto manifest = m_sdkPlugin->manifest();
    if (!manifest)
        return CameraDiagnostics::PluginErrorResult("Can't deserialize Plugin Manifest");

    nx::analytics::DescriptorManager descriptorManager(commonModule());
    descriptorManager.updateFromPluginManifest(*manifest);

    setManifest(*manifest);
    saveProperties();

    return CameraDiagnostics::NoErrorResult();
}

QString AnalyticsPluginResource::libName() const
{
    if (!NX_ASSERT(m_sdkPlugin))
        return QString();

    return m_sdkPlugin->libName();
}

} // namespace nx::vms::server::resource
