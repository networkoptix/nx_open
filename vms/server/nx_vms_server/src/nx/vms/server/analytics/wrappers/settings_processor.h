#pragma once

#include <optional>
#include <functional>

#include <nx/sdk/ptr.h>
#include <nx/sdk/i_settings_response.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/helpers/to_string.h>

#include <nx/utils/range_adapters.h>

#include <nx/vms/server/sdk_support/types.h>
#include <nx/vms/server/sdk_support/file_utils.h>
#include <nx/vms/server/sdk_support/conversion_utils.h>

#include <nx/vms/server/analytics/wrappers/types.h>
#include <nx/vms/server/analytics/wrappers/sdk_object_description.h>

#include <nx/vms/server/sdk_support/result_holder.h>
#include <nx/vms/server/sdk_support/error.h>

namespace nx::vms::server::analytics::wrappers {

class SettingsProcessor
{
    using SettingsResponseHolder = sdk_support::ResultHolder<const sdk::ISettingsResponse*>;
    using SettingsResponseFetcher = std::function<SettingsResponseHolder()>;

public:
    using SdkErrorHandler = std::function<void(const sdk_support::Error&)>;
    using ViolationHandler = std::function<void(const Violation& violation)>;

    SettingsProcessor(
        DebugSettings debugSettings,
        SdkObjectDescription sdkObjectDescription,
        SdkErrorHandler sdkErrorHandler,
        ViolationHandler violationHandler)
        :
        m_debugSettings(std::move(debugSettings)),
        m_sdkObjectDescription(std::move(sdkObjectDescription)),
        m_errorHandler(std::move(sdkErrorHandler)),
        m_violationHandler(std::move(violationHandler))
    {
    }

    template<typename SdkObject>
    std::optional<sdk_support::SdkSettingsResponse> pluginSideSettings(
        const SdkObject& sdkObject) const
    {
        if (!NX_ASSERT(sdkObject))
            return std::nullopt;

        return handleSettingsResponse("_plugin_side",
            [&sdkObject]() { return sdkObject->pluginSideSettings(); });
    }

    template<typename SdkObject>
    std::optional<sdk_support::SdkSettingsResponse> setSettings(
        SdkObject sdkObject,
        const sdk_support::SettingsValues& settings) const
    {
        if (!NX_ASSERT(sdkObject))
            return std::nullopt;

        const sdk::Ptr<const sdk::IStringMap> sdkSettings = prepareSettings(settings);
        if (!NX_ASSERT(sdkSettings))
            return std::nullopt;

        sdk_support::SdkSettingsResponse setSettingsResult =
            handleSettingsResponse(
                "_set_settings",
                [&sdkObject, sdkSettings]()
                {
                    return sdkObject->setSettings(sdkSettings.get());
                });

        if (!setSettingsResult.values) //< 4.0 plugin that doesn't provide values.
        {
            sdk_support::SdkSettingsResponse pluginSideSettingsResult =
                handleSettingsResponse(
                    "_plugin_side",
                    [&sdkObject]()
                    {
                        return sdkObject->pluginSideSettings();
                    });

            return mergeLegacySettingsResponses(setSettingsResult, pluginSideSettingsResult);
        }

        return setSettingsResult;
    }

private:
    sdk_support::SdkSettingsResponse handleSettingsResponse(
        const char* fileNameSuffix, SettingsResponseFetcher responseFetcher) const;

    sdk::Ptr<const sdk::IStringMap> prepareSettings(
        const sdk_support::SettingsValues& settings) const;

    std::optional<sdk_support::SettingsValues> loadSettingsFromFile() const;

    std::optional<sdk_support::SettingsValues> loadSettingsFromSpecificFile(
        sdk_support::FilenameGenerationOptions filenameGenerationOptions) const;

    std::optional<sdk_support::SdkSettingsResponse> loadPluginSideSettingsResponseFromFile(
        const char* fileNameSuffix) const;

    std::optional<sdk_support::SdkSettingsResponse> loadPluginSideSettingsResponseFromSpecificFile(
        const char* fileNameSuffix,
        sdk_support::FilenameGenerationOptions filenameGenerationOptions) const;

    static sdk_support::SdkSettingsResponse mergeLegacySettingsResponses(
        sdk_support::SdkSettingsResponse first,
        const sdk_support::SdkSettingsResponse& second);

private:
    DebugSettings m_debugSettings;
    SdkObjectDescription m_sdkObjectDescription;
    SdkErrorHandler m_errorHandler;
    ViolationHandler m_violationHandler;
};

} // namespace nx::vms::server::analytics::wrappers
