#pragma once

#include <nx/sdk/helpers/ptr.h>

#include <nx/vms/event/events/events_fwd.h>
#include <nx/vms/event/events/plugin_diagnostic_event.h>

#include <nx/vms/server/analytics/wrappers/base_plugin.h>
#include <nx/vms/server/analytics/wrappers/types.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/server/sdk_support/error.h>
#include <nx/vms/server/event/event_connector.h>
#include <nx/vms/server/analytics/wrappers/string_builder.h>
#include <nx/vms/server/analytics/wrappers/settings_processor.h>

namespace nx::vms::server::analytics::wrappers {

template<typename MainSdkObject, typename ManifestType>
class BaseEngine: public BasePlugin<MainSdkObject, ManifestType>
{
    using base_type = BasePlugin<MainSdkObject, ManifestType>;
public:
    BaseEngine(
        QnMediaServerModule* serverModule,
        sdk::Ptr<MainSdkObject> mainSdkObject,
        QString libName)
        :
        base_type(serverModule, mainSdkObject, libName)
    {
    }

    std::optional<sdk_support::ErrorMap> setSettings(const sdk_support::SettingMap& settings)
    {
        SettingsProcessor settingsProcessor(
            makeProcessorSettings(),
            sdkObjectDescription(),
            [this](const sdk_support::Error& error)
            {
                handleError(SdkMethod::setSettings, error, /*returnValue*/ nullptr);
            });

        return settingsProcessor.setSettings(sdkObject(), settings);
    }

    std::optional<sdk_support::SettingsResponse> pluginSideSettings() const
    {
        DebugSettings settings = makeProcessorSettings();
        SettingsProcessor settingsProcessor(
            makeProcessorSettings(),
            sdkObjectDescription(),
            [this](const sdk_support::Error& error)
            {
                handleError(SdkMethod::pluginSideSettings, error, /*returnValue*/ nullptr);
            });

        return settingsProcessor.pluginSideSettings(sdkObject());
    }
};

} // namespace nx::vms::server::analytics::wrappers
