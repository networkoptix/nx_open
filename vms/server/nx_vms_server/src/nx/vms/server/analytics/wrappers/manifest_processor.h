#pragma once

#include <core/resource/camera_resource.h>

#include <nx/sdk/i_string.h>
#include <nx/sdk/ptr.h>

#include <nx/vms/server/sdk_support/file_utils.h>
#include <nx/vms/server/sdk_support/conversion_utils.h>
#include <nx/vms/server/analytics/wrappers/types.h>

#include <nx/vms/server/resource/analytics_plugin_resource.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/event/events/events_fwd.h>

#include <nx/fusion/serialization/json.h>

namespace nx::vms::server::analytics::wrappers {

using ManifestProcessorViolationHandler = std::function<void(Violation)>;

template<typename Manifest>
class ManifestProcessor
{

public:
    ManifestProcessor(
        DebugSettings debugSettings,
        SdkObjectDescription sdkObjectDescription,
        ManifestProcessorViolationHandler violationHandler,
        ProcessorErrorHandler errorHandler)
        :
        m_debugSettings(std::move(debugSettings)),
        m_sdkObjectDescription(std::move(sdkObjectDescription)),
        m_violationHandler(std::move(violationHandler)),
        m_errorHandler(std::move(errorHandler))
    {
    }

    template<typename SdkObject>
    std::optional<Manifest> manifest(const SdkObject& sdkObject) const
    {
        if (!NX_ASSERT(sdkObject))
        {
            m_violationHandler({
                ViolationType::internalViolation,
                "SDK object is unavailable while retrieving manifest"});
            return std::nullopt;
        }

        sdk::Ptr<const sdk::IString> manifest = loadManifestStringFromFile();
        if (!manifest)
            manifest = loadManifestStringFromSdkObject(sdkObject);

        dumpManifestStringToFile(manifest);
        return processManifest(manifest);
    }

private:
    sdk::Ptr<const sdk::IString> loadManifestStringFromFile() const
    {
        if (m_debugSettings.substitutePath.isEmpty())
            return nullptr;

        const auto baseFilename = m_sdkObjectDescription.baseInputOutputFilename();
        const auto absoluteFilename = sdk_support::debugFileAbsolutePath(
            m_debugSettings.substitutePath,
            baseFilename + "_manifest.json");

        const std::optional<QString> result =
            sdk_support::loadStringFromFile(m_debugSettings.logTag, absoluteFilename);

        if (!result)
            return nullptr;

        return sdk_support::toSdkString(*result);
    }

    template<typename SdkObject>
    sdk::Ptr<const sdk::IString> loadManifestStringFromSdkObject(const SdkObject& sdkObject) const
    {
        if (!NX_ASSERT(sdkObject))
        {
            m_violationHandler({
                ViolationType::internalViolation,
                "SDK object is unavailable while retrieving manifest"});
            return nullptr;
        }

        const sdk_support::ResultHolder<const sdk::IString*> result = sdkObject->manifest();
        if (!result.isOk())
        {
            m_errorHandler(sdk_support::Error::fromResultHolder(result));
            return nullptr;
        }

        return result.value();
    }

    void dumpManifestStringToFile(const sdk::Ptr<const sdk::IString> manifestString) const
    {
        if (m_debugSettings.outputPath.isEmpty())
            return;

        const QString stringToDump = (manifestString && manifestString->str())
            ? QString(manifestString->str())
            : QString();

        const auto baseFilename = m_sdkObjectDescription.baseInputOutputFilename();
        const auto absoluteFilename = sdk_support::debugFileAbsolutePath(
            m_debugSettings.outputPath,
            baseFilename + "_manifest.json");

        const bool dumpIsSuccessful = sdk_support::dumpStringToFile(
            m_debugSettings.logTag,
            absoluteFilename,
            stringToDump);

        if (!dumpIsSuccessful)
        {
            m_violationHandler({
                ViolationType::internalViolation,
                "Unable to dump manifest to file"});
        }
    }

    std::optional<Manifest> processManifest(
        const sdk::Ptr<const sdk::IString> manifestString) const
    {
        if (!manifestString)
        {
            m_violationHandler(
                {ViolationType::nullManifest, /*violationDetails*/ QString()});
            return std::nullopt;
        }

        const char* const manifestCString = manifestString->str();
        if (!manifestCString)
        {
            m_violationHandler(
                {ViolationType::nullManifestString, /*violationDetails*/ QString()});
            return std::nullopt;
        }

        if (*manifestCString == '\0')
        {
            m_violationHandler(
                {ViolationType::emptyManifestString, /*violationDetails*/ QString()});
            return std::nullopt;
        }

        const std::optional<Manifest> deserializedManifest = deserializeManifest(manifestCString);

        if (!deserializedManifest)
            return std::nullopt;

        if (!validateManifest(*deserializedManifest))
            return std::nullopt;

        return deserializedManifest;
    }

    std::optional<Manifest> deserializeManifest(const QString& manifestString) const
    {
        Manifest result;

        QJsonValue jsonValue;
        if (!QJsonDetail::deserialize_json(manifestString.toUtf8(), &jsonValue))
        {
            m_violationHandler(
                {ViolationType::invalidJson, /*violationDetails*/ QString()});
            return std::nullopt;
        }

        QnJsonContext ctx;
        if (!QJson::deserialize(&ctx, jsonValue, &result))
        {
            m_violationHandler(
                {ViolationType::invalidJsonStructure, /*violationDetails*/ QString()});
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

        return errorList.empty();
    }

    QString buildManifestValidationDetails(
        const std::vector<nx::vms::api::analytics::ManifestError>& manifestErrors) const
    {
        QString result;
        for (int i = 0; i < manifestErrors.size(); ++i)
        {
            const auto& error = manifestErrors[i];
            result += lm("error: %1").args(toHumanReadableString(error.errorType));
            if (!error.additionalInfo.isEmpty())
                result += lm(", details: %1").args(error.additionalInfo);

            if (i < manifestErrors.size() - 1)
                result += ";\n";
        }

        return result;
    }

private:
    const DebugSettings m_debugSettings;
    const SdkObjectDescription m_sdkObjectDescription;
    ManifestProcessorViolationHandler m_violationHandler;
    ProcessorErrorHandler m_errorHandler;
};

} // namespace nx::vms::server::analytics::wrappers
