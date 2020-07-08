#pragma once

#include <algorithm>

#include <core/resource/camera_resource.h>

#include <nx/kit/utils.h>
#include <nx/sdk/i_string.h>
#include <nx/sdk/ptr.h>

#include <nx/vms/server/sdk_support/file_utils.h>
#include <nx/vms/server/sdk_support/conversion_utils.h>
#include <nx/vms/server/analytics/wrappers/sdk_object_description.h>
#include <nx/vms/server/analytics/wrappers/types.h>
#include <nx/vms/server/analytics/wrappers/manifest_converter.h>
#include <plugins/vms_server_plugins_ini.h>

#include <nx/vms/server/resource/analytics_engine_resource.h>

#include <nx/fusion/serialization/json.h>

namespace nx::vms::server::analytics::wrappers {

template<typename Manifest>
class ManifestProcessor
{
public:
    using ViolationHandler = std::function<void(Violation)>;
    using SdkErrorHandler = std::function<void(const sdk_support::Error&)>;
    using InternalErrorHandler = std::function<void(const QString& /*errorMessage*/)>;

    ManifestProcessor(
        DebugSettings debugSettings,
        SdkObjectDescription sdkObjectDescription,
        ViolationHandler violationHandler,
        SdkErrorHandler errorHandler,
        InternalErrorHandler internalErrorHandler)
        :
        m_manifestConverter(sdkObjectDescription, violationHandler),
        m_debugSettings(std::move(debugSettings)),
        m_sdkObjectDescription(std::move(sdkObjectDescription)),
        m_violationHandler(std::move(violationHandler)),
        m_sdkErrorHandler(std::move(errorHandler)),
        m_internalErrorHandler(std::move(internalErrorHandler))
    {
    }

    template<typename SdkObject>
    std::optional<Manifest> manifest(const SdkObject& sdkObject) const
    {
        if (!NX_ASSERT(sdkObject, kNullSdkObjectErrorMessage))
        {
            m_internalErrorHandler(kNullSdkObjectErrorMessage);
            return std::nullopt;
        }

        sdk::Ptr<const sdk::IString> manifestString = loadManifestStringFromFile();
        if (!manifestString)
            manifestString = loadManifestStringFromSdkObject(sdkObject);

        dumpManifestStringToFile(manifestString);
        return m_manifestConverter.convert(manifestString);
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
        if (!NX_ASSERT(sdkObject, kNullSdkObjectErrorMessage))
        {
            m_internalErrorHandler(kNullSdkObjectErrorMessage);
            return nullptr;
        }

        const sdk_support::ResultHolder<const sdk::IString*> result = sdkObject->manifest();
        if (!result.isOk())
        {
            m_sdkErrorHandler(sdk_support::Error::fromResultHolder(result));
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
            : "";

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
            m_internalErrorHandler(lm("Unable to dump manifest to file %2.").args(
                nx::kit::utils::toString(absoluteFilename)));
        }
    }

private:
    static constexpr const char* kNullSdkObjectErrorMessage =
        "SDK object to retrieve manifest from is null.";

    ManifestConverter<Manifest> m_manifestConverter;

    const DebugSettings m_debugSettings;
    const SdkObjectDescription m_sdkObjectDescription;
    ViolationHandler m_violationHandler;
    SdkErrorHandler m_sdkErrorHandler;
    InternalErrorHandler m_internalErrorHandler;
};

} // namespace nx::vms::server::analytics::wrappers
