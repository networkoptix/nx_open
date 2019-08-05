#pragma once

#include <core/resource/camera_resource.h>

#include <nx/sdk/i_string.h>
#include <nx/sdk/helpers/ptr.h>

#include <nx/vms/server/sdk_support/file_utils.h>
#include <nx/vms/server/sdk_support/conversion_utils.h>
#include <nx/vms/server/analytics/wrappers/types.h>

#include <nx/vms/server/resource/analytics_plugin_resource.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/event/events/events_fwd.h>

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
            m_violationHandler(Violation::internalViolation);
            return std::nullopt;
        }

        sdk::Ptr<const sdk::IString> manifest = loadManifestStringFromFile();
        if (!manifest)
            manifest = loadManifestStringFromSdkObject(sdkObject);

        dumpManifestStringToFile(manifest);
        return validateManifest(manifest);
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
            m_violationHandler(Violation::internalViolation);
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
            m_violationHandler(Violation::internalViolation);
    }

    std::optional<Manifest> validateManifest(
        const sdk::Ptr<const sdk::IString> manifestString) const
    {
        if (!manifestString)
            m_violationHandler(Violation::nullManifest);

        const char* const manifestCString = manifestString->str();
        if (!manifestCString)
            m_violationHandler(Violation::nullManifestString);

        if (*manifestCString == '\0')
            m_violationHandler(Violation::emptyManifestString);

        bool success = false;
        const auto manifest = QJson::deserialized<Manifest>(manifestCString, {}, &success);

        if (!success)
            m_violationHandler(Violation::deserializationError);

        return manifest;
    }

private:
    const DebugSettings m_debugSettings;
    const SdkObjectDescription m_sdkObjectDescription;
    ManifestProcessorViolationHandler m_violationHandler;
    ProcessorErrorHandler m_errorHandler;
};

} // namespace nx::vms::server::analytics::wrappers
