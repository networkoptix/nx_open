#pragma once

#include <optional>

#include <QtCore/QVariantMap>

#include <core/resource/resource_fwd.h>
#include <core/resource_management/resource_pool.h>

#include <media_server/media_server_module.h>

#include <nx/utils/log/log.h>
#include <nx/utils/member_detector.h>
#include <nx/utils/debug_helpers/debug_helpers.h>

#include <nx/sdk/analytics/i_uncompressed_video_frame.h>

#include <nx/fusion/model_functions.h>

#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/descriptors.h>

#include <nx/vms/server/resource/resource_fwd.h>
#include <nx/vms/server/resource/analytics_plugin_resource.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/analytics/debug_helpers.h>

#include <nx/vms/server/sdk_support/loggers.h>

#include <analytics/db/analytics_db_types.h>

#include <nx/sdk/i_string_map.h>
#include <nx/sdk/i_plugin_diagnostic_event.h>
#include <nx/sdk/helpers/device_info.h>
#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/helpers/error.h>
#include <plugins/settings.h>
#include <plugins/vms_server_plugins_ini.h>

class QnMediaServerModule;

namespace nx::vms::server::analytics { class SdkObjectFactory; }

namespace nx::vms::server::sdk_support {

nx::vms::api::analytics::PixelFormat fromSdkPixelFormat(
    nx::sdk::analytics::IUncompressedVideoFrame::PixelFormat sdkPixelFormat);

std::optional<nx::sdk::analytics::IUncompressedVideoFrame::PixelFormat> toSdkPixelFormat(
    nx::vms::api::analytics::PixelFormat pixelFormat);

/** @return Converted value, or, on error, AV_PIX_FMT_NONE, after failing an assertion. */
AVPixelFormat sdkToAvPixelFormat(
    nx::sdk::analytics::IUncompressedVideoFrame::PixelFormat sdkPixelFormat);

std::optional<nx::sdk::analytics::IUncompressedVideoFrame::PixelFormat> avPixelFormatToSdk(
    AVPixelFormat avPixelFormat);

template<typename Manifest>
std::optional<Manifest> loadManifestFromFile(const QString& filename)
{
    using nx::utils::log::Level;
    static const nx::utils::log::Tag kLogTag(
        QString("nx::vms::server::sdk_support::loadManifestFromFile"));

    auto logger =
        [&](Level level, const QString& message)
        {
            NX_UTILS_LOG(level, kLogTag) << lm("Loading manifest from file: %1: [%2]")
                .args(message, filename);
        };

    if (!NX_ASSERT(pluginsIni().analyticsManifestSubstitutePath[0]))
        return std::nullopt;

    const QDir dir(nx::utils::debug_helpers::debugFilesDirectoryPath(
        pluginsIni().analyticsManifestSubstitutePath));

    const QString fileData = analytics::debug_helpers::loadStringFromFile(
        dir.absoluteFilePath(filename), logger);

    if (fileData.isEmpty())
    {
        logger(Level::info, "Unable to read file");
        return std::nullopt;
    }

    bool success = false;
    const Manifest deserializedManifest =
        QJson::deserialized(fileData.toUtf8(), Manifest(), &success);

    if (success)
        return deserializedManifest;

    return std::nullopt;
}

template<typename Manifest, typename SdkObjectPtr>
std::optional<Manifest> manifestFromSdkObject(
    const SdkObjectPtr& sdkObject,
    std::unique_ptr<AbstractManifestLogger> logger = nullptr)
{
    using namespace nx::sdk;
    auto error = makePtr<Error>();
    const auto manifestStr = toPtr(sdkObject->manifest(error.get()));

    const auto log =
        [&logger](
            const QString& manifestString,
            Ptr<nx::sdk::IError> error,
            const QString& customError = QString())
        {
            if (logger)
                logger->log(manifestString, error, customError);
            return std::nullopt; //< Allows to call as `return log(...);` on error.
        };

    if (error->errorCode() != ErrorCode::noError)
        return log(QString(), error);

    if (!manifestStr)
    {
        return log(
            QString(),
            makePtr<Error>(ErrorCode::internalError, "No manifest (null IString)"));
    }

    const char* const rawString = manifestStr->str();

    if (!rawString)
    {
        return log(
            QString(),
            makePtr<Error>(ErrorCode::internalError, "No manifest (null IString::str())"));
    }

    if (rawString[0] == '\0')
    {
        return log(
            QString(),
            makePtr<Error>(ErrorCode::internalError, "No manifest (empty string)"));
    }

    bool success = false;
    const auto deserializedManifest = QJson::deserialized(rawString, Manifest(), &success);
    if (!success)
    {
        return log(
            rawString,
            makePtr<Error>(ErrorCode::internalError, "Unable to deserialize manifest"));
    }

    log(rawString, error);
    return deserializedManifest;
}

template<typename Manifest, typename SdkObjectPtr>
std::optional<Manifest> manifest(
    const SdkObjectPtr& sdkObject,
    const QString& substitutionFilename,
    std::unique_ptr<AbstractManifestLogger> logger = nullptr)
{
    const std::optional<Manifest> sdkObjectManifest =
        manifestFromSdkObject<Manifest>(sdkObject, std::move(logger));

    if (pluginsIni().analyticsManifestSubstitutePath[0])
    {
        const std::optional<Manifest> manifestSubstitution =
            loadManifestFromFile<Manifest>(substitutionFilename);

        if (manifestSubstitution)
            return manifestSubstitution;
    }

    return sdkObjectManifest;
}

template<typename Manifest, typename SdkObjectPtr>
std::optional<Manifest> manifest(
    const SdkObjectPtr& sdkObject,
    const QnVirtualCameraResourcePtr& device,
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engine,
    const nx::vms::server::resource::AnalyticsPluginResourcePtr& plugin,
    std::unique_ptr<AbstractManifestLogger> logger = nullptr)
{
    const auto substitutionFilename = analytics::debug_helpers::nameOfFileToDumpOrLoadData(
        device, engine, plugin, "_manifest.json");

    return manifest<Manifest>(sdkObject, substitutionFilename, std::move(logger));
}

template<typename ResourceType>
QnSharedResourcePointer<ResourceType> find(QnMediaServerModule* serverModule, const QString& id)
{
    if (!serverModule)
    {
        NX_ASSERT(false, "Can't access server module");
        return QnSharedResourcePointer<ResourceType>();
    }

    const auto resourcePool = serverModule->resourcePool();
    if (!resourcePool)
    {
        NX_ASSERT(false, "Can't access resource pool");
        return QnSharedResourcePointer<ResourceType>();
    }

    return resourcePool->getResourceById<ResourceType>(QnUuid(id));
}

analytics::SdkObjectFactory* getSdkObjectFactory(QnMediaServerModule* serverModule);

nx::sdk::Ptr<nx::sdk::DeviceInfo> deviceInfoFromResource(const QnVirtualCameraResourcePtr& device);

std::unique_ptr<nx::plugins::SettingsHolder> toSettingsHolder(const QVariantMap& settings);

nx::sdk::Ptr<nx::sdk::IStringMap> toIStringMap(const QVariantMap& map);
nx::sdk::Ptr<nx::sdk::IStringMap> toIStringMap(const QMap<QString, QString>& map);

/**
 * @param mapJson Json array of objects with string fields "name" and "value".
 * @return Null if the json is invalid, has unexpected structure (besides potentially added
 * unknown fields) or there are duplicate keys.
 */
nx::sdk::Ptr<nx::sdk::IStringMap> toIStringMap(const QString& mapJson);

QVariantMap fromIStringMap(const nx::sdk::IStringMap* map);

std::optional<nx::sdk::analytics::IUncompressedVideoFrame::PixelFormat>
    pixelFormatFromEngineManifest(
        const nx::vms::api::analytics::EngineManifest& manifest,
        const QString& engineLogLabel);

nx::vms::api::EventLevel fromPluginDiagnosticEventLevel(
    nx::sdk::IPluginDiagnosticEvent::Level level);

nx::sdk::Ptr<nx::sdk::analytics::ITimestampedObjectMetadata> createTimestampedObjectMetadata(
    const nx::analytics::db::DetectedObject& detectedObject,
    const nx::analytics::db::ObjectPosition& objectPosition);

nx::sdk::Ptr<nx::sdk::IList<nx::sdk::analytics::ITimestampedObjectMetadata>> createObjectTrack(
    const nx::analytics::db::DetectedObject& detectedObject);

nx::sdk::Ptr<nx::sdk::analytics::IUncompressedVideoFrame> createUncompressedVideoFrame(
    const CLVideoDecoderOutputPtr& frame,
    nx::vms::api::analytics::PixelFormat pixelFormat);

std::map<QString, QString> attributesMap(
    const nx::sdk::Ptr<const nx::sdk::analytics::IMetadata>& metadata);

} // namespace nx::vms::server::sdk_support
