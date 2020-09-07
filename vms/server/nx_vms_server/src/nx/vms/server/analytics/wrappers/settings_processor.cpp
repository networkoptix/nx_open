#include "settings_processor.h"

#include <plugins/vms_server_plugins_ini.h>

#include <nx/fusion/serialization/json.h>
#include <nx/vms/server/resource/camera.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/sdk_support/utils.h>
#include <nx/vms/server/interactive_settings/json_engine.h>
#include <nx/utils/string.h>

namespace nx::vms::server::analytics::wrappers {

static QString buildSettingsErrorString(
    const sdk_support::SettingsErrors& errors, const QString& prefix)
{
    QString result = prefix;
    for (const auto [key, value]: nx::utils::constKeyValueRange(errors))
        result += key + ": " + value + ";\n";

    return result;
}

static sdk_support::SdkSettingsResponse toSdkSettingsResponse(
    const sdk_support::ResultHolder<const sdk::ISettingsResponse*>& settingsResponse,
    Violation *outViolation)
{
    using namespace nx::vms::server::sdk_support;

    SdkSettingsResponse result;

    result.sdkError = Error::fromResultHolder(settingsResponse);

    const sdk::Ptr<const sdk::IRefCountable> value = settingsResponse.value();
    if (!value)
        return result;

    if (const auto settingsResponse = value->queryInterface<sdk::ISettingsResponse>())
    {
        result.values = SettingsValues();
        if (const sdk::Ptr<const sdk::IStringMap> settingsValues = settingsResponse->values())
            result.values = fromSdkStringMap<SettingsValues>(settingsValues);

        if (const sdk::Ptr<const sdk::IStringMap> settingsErrors = settingsResponse->errors())
            result.errors = fromSdkStringMap<SettingsErrors>(settingsErrors);

        if (const sdk::Ptr<const sdk::IString> settingsModel = settingsResponse->model())
        {
            const auto modelString = fromSdkString<QString>(settingsModel);
            QJsonParseError jsonParseError;
            QJsonDocument doc = QJsonDocument::fromJson(modelString.toUtf8(), &jsonParseError);
            if (jsonParseError.error != QJsonParseError::ParseError::NoError)
            {
                *outViolation = {ViolationType::invalidJson, jsonParseError.errorString()};
            }
            else if (!doc.isObject())
            {
                *outViolation = {
                    ViolationType::invalidJsonStructure,
                    "Settings Model is not a JSON object"};
            }
            else
            {
                result.model = doc.object();
            }
        }
    }
    else if (const auto settingsResponse0 = value->queryInterface<sdk::ISettingsResponse0>())
    {
        // This type of response may be returned by pluginSideSettings() of 4.0 plugins.

        result.values = SettingsValues();
        if (const sdk::Ptr<const sdk::IStringMap> settingsValues = settingsResponse0->values())
            result.values = fromSdkStringMap<SettingsValues>(settingsValues);

        if (const sdk::Ptr<const sdk::IStringMap> settingsErrors = settingsResponse0->errors())
            result.errors = fromSdkStringMap<SettingsErrors>(settingsErrors);
    }
    else if (const auto stringMap = value->queryInterface<nx::sdk::IStringMap>())
    {
        // This type of response may be returned by setSettings() of 4.0 plugins.
        result.errors = fromSdkStringMap<SettingsErrors>(stringMap);
    }

    return result;
}

sdk_support::SdkSettingsResponse SettingsProcessor::handleSettingsResponse(
    const char* fileNameSuffix, SettingsResponseFetcher responseFetcher) const
{
    const SettingsResponseHolder sdkSettingsResponseHolder = responseFetcher();

    Violation violation;

    sdk_support::SdkSettingsResponse pluginSideSettingsResponseFromPlugin =
        toSdkSettingsResponse(sdkSettingsResponseHolder, &violation);

    if (violation.type != ViolationType::undefined)
        m_violationHandler(violation);

    if (!m_debugSettings.outputPath.isEmpty())
    {
        const auto baseFilename = m_sdkObjectDescription.baseInputOutputFilename();
        const auto absoluteFilename = sdk_support::debugFileAbsolutePath(
            m_debugSettings.outputPath,
            baseFilename + "_actual_settings_response" + fileNameSuffix + ".json");

        // Always dump original settings response (i.e. the one received from plugin, not the one
        // loaded from the file).
        sdk_support::dumpStringToFile(
            m_debugSettings.logTag,
            absoluteFilename,
            nx::utils::formatJsonString(QJson::serialized(pluginSideSettingsResponseFromPlugin)));
    }

    sdk_support::SdkSettingsResponse pluginSideSettingsResponse =
        pluginSideSettingsResponseFromPlugin;
    if (const std::optional<sdk_support::SdkSettingsResponse> pluginSideSettingsResponseFromFile =
        loadPluginSideSettingsResponseFromFile(fileNameSuffix))
    {
        pluginSideSettingsResponse = *pluginSideSettingsResponseFromFile;
    }

    if (!pluginSideSettingsResponse.sdkError.isOk())
        m_errorHandler(pluginSideSettingsResponse.sdkError);

    if (!pluginSideSettingsResponse.errors.isEmpty())
    {
        m_errorHandler({
            sdk::ErrorCode::otherError,
            buildSettingsErrorString(
                pluginSideSettingsResponse.errors, "Errors occurred while applying settings:\n")});
    }

    return pluginSideSettingsResponse;
}

std::optional<sdk_support::SdkSettingsResponse>
SettingsProcessor::loadPluginSideSettingsResponseFromFile(const char* fileNameSuffix) const
{
    using namespace nx::vms::server::sdk_support;

    std::optional<sdk_support::SdkSettingsResponse> result =
        loadPluginSideSettingsResponseFromSpecificFile(
        fileNameSuffix,
        FilenameGenerationOption::engineSpecific | FilenameGenerationOption::deviceSpecific);

    if (result)
        return result;

    result = loadPluginSideSettingsResponseFromSpecificFile(
        fileNameSuffix, FilenameGenerationOption::deviceSpecific);
    if (result)
        return result;

    result = loadPluginSideSettingsResponseFromSpecificFile(
        fileNameSuffix, FilenameGenerationOption::engineSpecific);
    if (result)
        return result;

    return loadPluginSideSettingsResponseFromSpecificFile(fileNameSuffix,
        FilenameGenerationOptions());
}

std::optional<sdk_support::SdkSettingsResponse>
SettingsProcessor::loadPluginSideSettingsResponseFromSpecificFile(
    const char* fileNameSuffix,
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
            filenameGenerationOptions)) + "_settings_response" + fileNameSuffix + ".json";

    std::optional<QString> settingsString = sdk_support::loadStringFromFile(
        nx::utils::log::Tag(this),
        settingsFilename);

    if (!settingsString)
        return std::nullopt;

    bool isValid = false;
    const auto& result = QJson::deserialized<sdk_support::SdkSettingsResponse>(
        settingsString->toUtf8(), /*defaultValue*/ {}, &isValid);

    if (!isValid)
    {
        NX_WARNING(this,
            "Unable to parse data from file %1: loaded JSON string is malformed",
            settingsFilename);
        return std::nullopt;
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
        nx::utils::log::Tag(this),
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
