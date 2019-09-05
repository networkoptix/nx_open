#pragma once

#include <optional>

#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/plugin_manifest.h>
#include <nx/vms/server/analytics/engine_handler.h>
#include <nx/vms/server/analytics/wrappers/fwd.h>
#include <nx/vms/server/resource/resource_fwd.h>

#include <nx/sdk/analytics/i_engine.h>
#include <nx/sdk/ptr.h>

#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server::resource {

class AnalyticsEngineResource:
    public nx::vms::common::AnalyticsEngineResource,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
    Q_OBJECT

    using base_type = nx::vms::common::AnalyticsEngineResource;

public:
    AnalyticsEngineResource(QnMediaServerModule* serverModule);

    void setSdkEngine(analytics::wrappers::EnginePtr sdkEngine);

    analytics::wrappers::EnginePtr sdkEngine() const;

    virtual QVariantMap settingsValues() const override;
    virtual void setSettingsValues(const QVariantMap& values) override;

    bool sendSettingsToSdkEngine();

    QString libName() const;

signals:
    void engineInitialized(const AnalyticsEngineResourcePtr& engine);

private:
    std::optional<nx::vms::api::analytics::PluginManifest> pluginManifest() const;

    std::optional<QVariantMap> loadSettingsFromFile() const;

protected:
    virtual CameraDiagnostics::Result initInternal() override;

private:
    // ATTENTION: The field order is essential: EngineHandler must survive IEngine.
    nx::sdk::Ptr<analytics::EngineHandler> m_handler;
    analytics::wrappers::EnginePtr m_sdkEngine;
};

} // namespace nx::vms::server::resource
