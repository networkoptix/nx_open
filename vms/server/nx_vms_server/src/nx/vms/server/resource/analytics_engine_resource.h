#pragma once

#include <optional>

#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/plugin_manifest.h>
#include <nx/vms/server/analytics/engine_handler.h>
#include <nx/vms/server/analytics/wrappers/fwd.h>
#include <nx/vms/server/analytics/settings.h>
#include <nx/vms/server/resource/resource_fwd.h>
#include <nx/vms/server/sdk_support/types.h>

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

    analytics::SettingsResponse getSettings();
    analytics::SettingsResponse setSettings(const analytics::SetSettingsRequest& settingsRequest);
    analytics::SettingsResponse setSettings();

    QString libName() const;

signals:
    void engineInitialized(const AnalyticsEngineResourcePtr& engine);

private:
    std::optional<nx::vms::api::analytics::PluginManifest> pluginManifest() const;

    analytics::SettingsContext currentSettingsContext() const;

    analytics::SettingsContext updateSettingsContext(
        const analytics::SettingsContext& settingsContext,
        const api::analytics::SettingsValues& requestValues,
        const sdk_support::SdkSettingsResponse& sdkSettingsResponse);

    api::analytics::SettingsValues prepareSettings(
        const analytics::SettingsContext& settingsContext,
        const api::analytics::SettingsValues& settings) const;

    analytics::SettingsResponse prepareSettingsResponse(
        const analytics::SettingsContext& settingsContext,
        const sdk_support::SdkSettingsResponse& sdkSettingsResponse) const;

    analytics::SettingsResponse setSettingsInternal(
        const analytics::SetSettingsRequest& settingsRequest,
        bool isInitialSettings);

    sdk_support::SettingsValues storedSettingsValues() const;
    sdk_support::SettingsModel storedSettingsModel() const;

    void setSettingsValues(const api::analytics::SettingsValues& settingsValues);
    void setSettingsModel(const api::analytics::SettingsModel& settingsModel);

protected:
    virtual CameraDiagnostics::Result initInternal() override;

private:
    // ATTENTION: The field order is essential: EngineHandler must survive IEngine.
    nx::sdk::Ptr<analytics::EngineHandler> m_handler;
    analytics::wrappers::EnginePtr m_sdkEngine;
};

} // namespace nx::vms::server::resource
