#pragma once

#include <nx/sdk/ptr.h>

#include <nx/vms/event/events/events_fwd.h>
#include <nx/vms/event/events/plugin_diagnostic_event.h>

#include <nx/vms/server/analytics/wrappers/sdk_object_with_manifest.h>
#include <nx/vms/server/analytics/wrappers/types.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/server/sdk_support/error.h>
#include <nx/vms/server/event/event_connector.h>
#include <nx/vms/server/analytics/wrappers/string_builder.h>
#include <nx/vms/server/analytics/wrappers/settings_processor.h>

namespace nx::vms::server::analytics::wrappers {

template<typename MainSdkObject, typename ManifestType>
class SdkObjectWithSettings: public SdkObjectWithManifest<MainSdkObject, ManifestType>
{
    using base_type = SdkObjectWithManifest<MainSdkObject, ManifestType>;
public:
    SdkObjectWithSettings(
        QnMediaServerModule* serverModule,
        sdk::Ptr<MainSdkObject> mainSdkObject,
        QString libName)
        :
        base_type(serverModule, mainSdkObject, libName)
    {
    }

    std::optional<sdk_support::ErrorMap> setSettings(const sdk_support::SettingMap& settings)
    {
        nx::utils::TimedGuard guard = makeTimedGuard(SdkMethod::setSettings);

        SettingsProcessor settingsProcessor(
            this->makeSettingsProcessorSettings(),
            this->sdkObjectDescription(),
            [this](const sdk_support::Error& error)
            {
                this->handleError(SdkMethod::setSettings, error, /*returnValue*/ nullptr);
            });

        return settingsProcessor.setSettings(this->sdkObject(), settings);
    }

    std::optional<sdk_support::SettingsResponse> pluginSideSettings() const
    {
        nx::utils::TimedGuard guard = makeTimedGuard(SdkMethod::pluginSideSettings);

        DebugSettings settings = makeSettingsProcessorSettings();
        SettingsProcessor settingsProcessor(
            this->makeSettingsProcessorSettings(),
            this->sdkObjectDescription(),
            [this](const sdk_support::Error& error)
            {
                this->handleError(SdkMethod::pluginSideSettings, error, /*returnValue*/ nullptr);
            });

        return settingsProcessor.pluginSideSettings(this->sdkObject());
    }

protected:
    virtual DebugSettings makeSettingsProcessorSettings() const = 0;
};

} // namespace nx::vms::server::analytics::wrappers
