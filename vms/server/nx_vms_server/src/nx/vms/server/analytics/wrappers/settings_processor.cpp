#include "settings_processor.h"

#include <nx/fusion/serialization/json.h>

namespace nx::vms::server::analytics::wrappers {

sdk::Ptr<const sdk::IStringMap> SettingsProcessor::prepareSettings(
    const sdk_support::SettingMap& settings) const
{
    const auto sdkSettings = sdk::makePtr<sdk::StringMap>();
    for (auto it = settings.begin(); it != settings.end(); ++it)
        sdkSettings->setItem(it.key().toStdString(), QJson::serialize(it.value()).toStdString());

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

QString SettingsProcessor::buildSettingsErrorString(
    const sdk_support::ErrorMap& errors, const QString& prefix) const
{
    QString result = prefix;
    for (const auto [key, value]: nx::utils::constKeyValueRange(errors))
        result += key + ": " + value + ";\n";

    return result;
}

} // namespace nx::vms::server::analytics::wrappers
