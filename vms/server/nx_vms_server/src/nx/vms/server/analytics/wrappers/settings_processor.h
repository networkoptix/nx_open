#pragma once

#include <optional>

#include <nx/sdk/ptr.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/helpers/to_string.h>

#include <nx/utils/range_adapters.h>

#include <nx/vms/server/sdk_support/types.h>
#include <nx/vms/server/sdk_support/file_utils.h>
#include <nx/vms/server/sdk_support/conversion_utils.h>

#include <nx/vms/server/analytics/wrappers/types.h>

#include <nx/vms/server/sdk_support/result_holder.h>
#include <nx/vms/server/sdk_support/error.h>

namespace nx::vms::server::analytics::wrappers {

class SettingsProcessor
{
public:
    SettingsProcessor(
        DebugSettings debugSettings,
        SdkObjectDescription sdkObjectDescription,
        ProcessorErrorHandler errorHandler)
        :
        m_debugSettings(std::move(debugSettings)),
        m_sdkObjectDescription(std::move(sdkObjectDescription)),
        m_errorHandler(std::move(errorHandler))
    {
    }

    template<typename SdkObject>
    std::optional<sdk_support::SettingsResponse> pluginSideSettings(
        const SdkObject& sdkObject) const
    {
        const sdk_support::ResultHolder<const sdk::ISettingsResponse*> result =
            sdkObject->pluginSideSettings();

        if (!result.isOk())
            m_errorHandler(sdk_support::Error::fromResultHolder(result));

        const sdk::Ptr<const sdk::ISettingsResponse> sdkSettingsResponse = result.value();
        if (!sdkSettingsResponse)
        {
            NX_DEBUG(
                m_debugSettings.logTag,
                "Got a null settings response while obtaining settings from [%1]",
                m_sdkObjectDescription.descriptionString());

            return std::nullopt;
        }

        const auto settingValues = sdkSettingsResponse->values();
        if (!settingValues)
        {
            NX_DEBUG(
                m_debugSettings.logTag,
                "Got a null settings value map from [%1]",
                m_sdkObjectDescription.descriptionString());
        }

        sdk_support::SettingsResponse settingsResponse{
            sdk_support::fromSdkStringMap<sdk_support::SettingMap>(settingValues),
            sdk_support::fromSdkStringMap<sdk_support::ErrorMap>(sdkSettingsResponse->errors())};

        if (!settingsResponse.errors.isEmpty())
        {
            m_errorHandler({
                sdk::ErrorCode::otherError,
                buildSettingsErrorString(
                    settingsResponse.errors,
                    "Errors occurred while applying settings:\n")});
        }

        return settingsResponse;
    }

    template<typename SdkObject>
    std::optional<sdk_support::ErrorMap> setSettings(
        SdkObject sdkObject,
        const sdk_support::SettingMap& settings) const
    {
        if (!NX_ASSERT(sdkObject))
            return std::nullopt;

        sdk::Ptr<const sdk::IStringMap> sdkSettings = prepareSettings(settings);
        if (!NX_ASSERT(sdkSettings))
            return std::nullopt;

        const sdk_support::ResultHolder<const sdk::IStringMap*> result =
            sdkObject->setSettings(sdkSettings.get());

        if (!result.isOk())
        {
            m_errorHandler(sdk_support::Error::fromResultHolder(result));
            return std::nullopt;
        }

        const auto errorMap = sdk_support::fromSdkStringMap<sdk_support::ErrorMap>(result.value());
        if (!errorMap.isEmpty())
        {
            m_errorHandler({
                sdk::ErrorCode::otherError,
                buildSettingsErrorString(
                    errorMap,
                    "Errors occurred while retrieving settings:\n")});
        }

        return sdk_support::fromSdkStringMap<sdk_support::ErrorMap>(result.value());
    }

private:
    sdk::Ptr<const sdk::IStringMap> prepareSettings(const sdk_support::SettingMap& settings) const
    {
        const auto sdkSettings = sdk::makePtr<sdk::StringMap>();
        for (const auto& [key, value]: nx::utils::constKeyValueRange(settings))
            sdkSettings->setItem(key.toStdString(), value.toString().toStdString());

        if (!m_debugSettings.outputPath.isEmpty())
        {
            const auto baseFilename = m_sdkObjectDescription.baseInputOutputFilename();
            const auto absoluteFilename = sdk_support::debugFileAbsolutePath(
                m_debugSettings.outputPath,
                baseFilename + "_effective_settings.json");

            const bool dumpIsSuccessful = sdk_support::dumpStringToFile(
                m_debugSettings.logTag,
                absoluteFilename,
                QString::fromStdString(sdk::toJsonString(sdkSettings.get())));
        }

        return sdkSettings;
    }

    QString buildSettingsErrorString(
        const sdk_support::ErrorMap& errors,
        const QString& prefix) const
    {
        QString result = prefix;
        for (const auto& [key, value]: nx::utils::constKeyValueRange(errors))
            result += key + ": " + value + ";\n";

        return result;
    }

private:
    DebugSettings m_debugSettings;
    SdkObjectDescription m_sdkObjectDescription;
    ProcessorErrorHandler m_errorHandler;
};

} // namespace nx::vms::server::analytics::wrappers
