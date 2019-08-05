#pragma once

#include <nx/vms/server/analytics/wrappers/base_plugin.h>

#include <nx/vms/server/resource/resource_fwd.h>

namespace nx::vms::server::analytics::wrappers {

class Engine;

class Plugin: public BasePlugin<sdk::analytics::IPlugin, api::analytics::PluginManifest>
{
    using base_type = BasePlugin<sdk::analytics::IPlugin, api::analytics::PluginManifest>;

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

    std::shared_ptr<wrappers::Engine> createEngine(
        const resource::AnalyticsEngineResourcePtr engineResource);

protected:
    virtual DebugSettings makeProcessorSettings() const override;

    virtual SdkObjectDescription sdkObjectDescription() const override;

private:
    resource::AnalyticsPluginResourcePtr pluginResource() const;

private:
    const QString m_libraryName;
    QWeakPointer<resource::AnalyticsPluginResource> m_pluginResource;
};

} // namespace nx::vms::server::analytics::wrappers
