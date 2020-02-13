#pragma once

#include <nx/sdk/analytics/i_plugin.h>
#include <nx/vms/server/analytics/wrappers/fwd.h>
#include <nx/vms/server/analytics/wrappers/sdk_object_with_manifest.h>

#include <nx/vms/server/resource/resource_fwd.h>

namespace nx::vms::server::analytics::wrappers {

class Engine;

class Plugin: public SdkObjectWithManifest<sdk::analytics::IPlugin, api::analytics::PluginManifest>
{
    using base_type =
        SdkObjectWithManifest<sdk::analytics::IPlugin, api::analytics::PluginManifest>;

public:
    Plugin(
        QnMediaServerModule* serverModule,
        QWeakPointer<resource::AnalyticsPluginResource> pluginResource,
        sdk::Ptr<sdk::analytics::IPlugin> sdkPlugin,
        QString libraryName);

    Plugin(
        QnMediaServerModule* serverModule,
        sdk::Ptr<sdk::analytics::IPlugin> sdkPlugin,
        QString libraryName);

    wrappers::EnginePtr createEngine(const resource::AnalyticsEngineResourcePtr engineResource);

protected:
    virtual DebugSettings makeManifestProcessorDebugSettings() const override;

    virtual SdkObjectDescription sdkObjectDescription() const override;

private:
    resource::AnalyticsPluginResourcePtr pluginResource() const;

private:
    const QString m_libraryName;
    QWeakPointer<resource::AnalyticsPluginResource> m_pluginResource;
};

} // namespace nx::vms::server::analytics::wrappers
