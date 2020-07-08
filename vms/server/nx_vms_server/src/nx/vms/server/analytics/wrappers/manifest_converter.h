#pragma once

#include <nx/vms/server/analytics/wrappers/sdk_object_description.h>

namespace nx::vms::server::analytics::wrappers {

using ViolationHandler = std::function<void(Violation)>;
using SdkErrorHandler = std::function<void(const sdk_support::Error&)>;
using InternalErrorHandler = std::function<void(const QString& /*errorMessage*/)>;

template<typename Manifest>
class ManifestConverter
{
public:
    ManifestConverter(
        SdkObjectDescription sdkObjectDescription,
        ViolationHandler violationHandler)
        :
        m_sdkObjectDescription(std::move(sdkObjectDescription)),
        m_violationHandler(std::move(violationHandler))
    {
    }

    std::optional<Manifest> convert(
        const sdk::Ptr<const sdk::IString> manifestString) const
    {
        if (!manifestString)
        {
            m_violationHandler({ViolationType::nullManifest, /*violationDetails*/ ""});
            return std::nullopt;
        }

        const char* const manifestCString = manifestString->str();
        if (!manifestCString)
        {
            m_violationHandler({ViolationType::nullManifestString, /*violationDetails*/ ""});
            return std::nullopt;
        }

        if (*manifestCString == '\0')
        {
            m_violationHandler({ViolationType::emptyManifestString, /*violationDetails*/ ""});
            return std::nullopt;
        }

        const std::optional<Manifest> deserializedManifest = deserializeManifest(manifestCString);

        if (!deserializedManifest)
            return std::nullopt;

        if (!validateManifest(*deserializedManifest))
            return std::nullopt;

        return deserializedManifest;
    }

private:
    std::optional<Manifest> deserializeManifest(const QString& manifestString) const
    {
        Manifest result;

        QJsonValue jsonValue;
        if (!QJsonDetail::deserialize_json(manifestString.toUtf8(), &jsonValue))
        {
            m_violationHandler({ViolationType::invalidJson, /*violationDetails*/ ""});
            return std::nullopt;
        }

        QnJsonContext ctx;
        if (!QJson::deserialize(&ctx, jsonValue, &result))
        {
            m_violationHandler({ViolationType::invalidJsonStructure, /*violationDetails*/ ""});
            return std::nullopt;
        }

        return result;
    }

    bool validateManifest(const Manifest& manifest) const
    {
        const auto errorList = validate(manifest);

        if (!errorList.empty())
        {
            m_violationHandler({
                ViolationType::manifestLogicalError,
                buildManifestValidationDetails(errorList)});
        }

        return std::none_of(errorList.cbegin(), errorList.cend(),
            [](const auto& manifestError) { return isCriticalManifestError(manifestError); });
    }

    static QString buildManifestValidationDetails(
        const std::vector<api::analytics::ManifestError>& manifestErrors)
    {
        QString result;
        for (int i = 0; i < (int) manifestErrors.size(); ++i)
        {
            const auto& error = manifestErrors[i];
            result += lm("error: %1").args(error.errorType);
            if (!error.additionalInfo.isEmpty())
                result += lm(", details: %1").args(error.additionalInfo);

            if (i < (int) manifestErrors.size() - 1)
                result += "\n";
        }

        return result;
    }

    static bool isCriticalManifestError(
        const api::analytics::ManifestError& manifestError)
    {
        if (manifestError.errorType == api::analytics::ManifestErrorType::noError)
            return false;

        if (pluginsIni().enableStrictManifestValidationMode)
            return true;

        if (manifestError.errorType == api::analytics::ManifestErrorType::emptyPluginId)
            return true;

        return false;
    }

private:
    const SdkObjectDescription m_sdkObjectDescription;
    ViolationHandler m_violationHandler;
};


} // namespace nx::vms::server::analytics::wrappers
