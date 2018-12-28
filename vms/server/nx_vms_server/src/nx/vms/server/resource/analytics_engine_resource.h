#pragma once

#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/plugin_manifest.h>
#include <nx/vms/server/analytics/engine_handler.h>

#include <nx/sdk/analytics/i_engine.h>
#include <nx/sdk/helpers/ptr.h>

#include <nx/vms/server/sdk_support/loggers.h>
#include <nx/vms/server/server_module_aware.h>

#include <nx/utils/std/optional.h>

namespace nx::vms::server::resource {

class AnalyticsEngineResource:
    public nx::vms::common::AnalyticsEngineResource,
    public nx::vms::server::ServerModuleAware
{
    Q_OBJECT

    using base_type = nx::vms::common::AnalyticsEngineResource;

public:
    AnalyticsEngineResource(QnMediaServerModule* serverModule);

    // TODO: #dmishin: Rename sdkEngine to engine.
    void setSdkEngine(nx::sdk::Ptr<nx::sdk::analytics::IEngine> sdkEngine);

    nx::sdk::Ptr<nx::sdk::analytics::IEngine> sdkEngine() const;

    virtual QVariantMap settingsValues() const override;
    virtual void setSettingsValues(const QVariantMap& values) override;

    bool sendSettingsToSdkEngine();

signals:
    void engineInitialized(const AnalyticsEngineResourcePtr& engine);

private:
    std::optional<nx::vms::api::analytics::PluginManifest> pluginManifest() const;
    std::unique_ptr<sdk_support::AbstractManifestLogger> makeLogger() const;

protected:
    virtual CameraDiagnostics::Result initInternal() override;

private:
    std::unique_ptr<analytics::EngineHandler> m_handler;
    nx::sdk::Ptr<nx::sdk::analytics::IEngine> m_sdkEngine;
};

} // namespace nx::vms::server::resource
