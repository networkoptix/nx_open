#include "settings_processor.h"

#include <plugins/vms_server_plugins_ini.h>

#include <nx/fusion/serialization/json.h>
#include <nx/vms/server/resource/camera.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/sdk_support/utils.h>
#include <nx/vms/server/interactive_settings/json_engine.h>

namespace nx::vms::server::analytics::wrappers {

static QString buildSettingsErrorString(
    const sdk_support::SettingsErrors& errors, const QString& prefix)
{
    QString result = prefix;
    for (const auto [key, value]: nx::utils::constKeyValueRange(errors))
        result += key + ": " + value + ";\n";

    return result;
}

sdk_support::SdkSettingsResponse SettingsProcessor::handleSettingsResponse(
    SettingsResponseFetcher responseFetcher) const
{
    const SettingsResponseHolder sdkSettingsResponseHolder = responseFetcher();

    sdk_support::SdkSettingsResponse result =
        sdk_support::toSdkSettingsResponse(sdkSettingsResponseHolder);

    if (!result.sdkError.isOk())
        m_errorHandler(result.sdkError);

    if (!result.errors.isEmpty())
    {
        m_errorHandler({
            sdk::ErrorCode::otherError,
            buildSettingsErrorString(
                result.errors, "Errors occurred while applying settings:\n")});
    }

    return result;
}

std::optional<sdk_support::SettingsValues> SettingsProcessor::loadSettingsFromFile() const
{
    using namespace nx::vms::server::sdk_support;

    std::optional<sdk_support::SettingsValues> result = loadSettingsFromSpecificFile(
        FilenameGenerationOption::engineSpecific | FilenameGenerationOption::deviceSpecific);

    if (result)
        return result;

    result = loadSettingsFromSpecificFile(FilenameGenerationOption::deviceSpecific);
    if (result)
        return result;

    result = loadSettingsFromSpecificFile(FilenameGenerationOption::engineSpecific);
    if (result)
        return result;

    return loadSettingsFromSpecificFile(FilenameGenerationOptions());
}

std::optional<QJsonObject> SettingsProcessor::loadSettingsFromSpecificFile(
    sdk_support::FilenameGenerationOptions filenameGenerationOptions) const
{
    if (pluginsIni().analyticsSettingsSubstitutePath[0] == 0)
        return std::nullopt;

    const QString settingsFilename = sdk_support::debugFileAbsolutePath(
        pluginsIni().analyticsSettingsSubstitutePath,
        sdk_support::baseNameOfFileToDumpOrLoadData(
            m_sdkObjectDescription.plugin(),
            m_sdkObjectDescription.engine(),
            m_sdkObjectDescription.device(),
            filenameGenerationOptions)) + "_settings.json";

    std::optional<QString> settingsString = sdk_support::loadStringFromFile(
        nx::utils::log::Tag(typeid(this)),
        settingsFilename);

    if (!settingsString)
        return std::nullopt;

    return sdk_support::toQJsonObject(*settingsString);
}

static sdk::Ptr<const sdk::IStringMap> toSdkStringMap(const sdk_support::SettingsValues& settings)
{
    const auto sdkSettings = sdk::makePtr<sdk::StringMap>();
    for (auto it = settings.begin(); it != settings.end(); ++it)
    {
        sdkSettings->setItem(
            it.key().toStdString(),
            (it->isString() ? it->toString() : QJson::serialize(it.value())).toStdString());
    }

    return sdkSettings;
}

sdk::Ptr<const sdk::IStringMap> SettingsProcessor::prepareSettings(
    const sdk_support::SettingsValues& settings) const
{
    sdk_support::SettingsValues effectiveSettings = settings;
    if (const std::optional<sdk_support::SettingsValues> settingsFromFile = loadSettingsFromFile())
        effectiveSettings = *settingsFromFile;

    const sdk::Ptr<const sdk::IStringMap> sdkSettings = toSdkStringMap(effectiveSettings);

    if (!m_debugSettings.outputPath.isEmpty())
    {
        const auto baseFilename = m_sdkObjectDescription.baseInputOutputFilename();
        const auto absoluteFilename = sdk_support::debugFileAbsolutePath(
            m_debugSettings.outputPath,
            baseFilename + "_effective_settings.json");

        sdk_support::dumpStringToFile(
            m_debugSettings.logTag,
            absoluteFilename,
            QString::fromStdString(sdk::toJsonString(sdkSettings.get())));
    }

    return sdkSettings;
}

/*static*/
sdk_support::SdkSettingsResponse SettingsProcessor::mergeLegacySettingsResponses(
    sdk_support::SdkSettingsResponse first,
    const sdk_support::SdkSettingsResponse& second)
{
    // This method is used only for 4.0 plugins that are unable to return the values and the model
    // from setSettings().

    if (!second.sdkError.isOk())
        first.sdkError = second.sdkError;

    first.values = second.values;
    for (const auto& [name, value]: nx::utils::constKeyValueRange(second.errors))
        first.errors[name] = value;

    return first;
}

} // namespace nx::vms::server::analytics::wrappers
