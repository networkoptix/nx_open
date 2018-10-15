#pragma once

#include <QtCore/QVariantMap>

#include <core/resource/resource_fwd.h>

#include <nx/utils/std/optional.h>
#include <nx/utils/log/log_level.h>

#include <nx/sdk/common.h>
#include <nx/sdk/analytics/uncompressed_video_frame.h>

#include <nx/vms/api/analytics/engine_manifest.h>

#include <nx/plugins/settings.h>

class QnMediaServerModule;
namespace nx::mediaserver::analytics {

class SdkObjectPool;

} // namespace nx::mediaserver::analytics

namespace nx::mediaserver::sdk_support {

template<typename ManifestType>
std::optional<ManifestType> manifest(const char* const manifestString)
{
    bool success = false;
    auto deserializedManifest = QJson::deserialized(manifestString, ManifestType(), &success);
    if (!success)
        return std::nullopt;

    return deserializedManifest;
}

template<typename ManifestType, typename SdkObjectPtr>
std::optional<ManifestType> manifest(const SdkObjectPtr& sdkObject)
{
    nx::sdk::Error error;

    // TODO: #dmishin RAII wrapper for manifest. Or change manifest interface
    // from char* to PluginInterface.
    auto deleter = [&sdkObject](const char* manifestMem) { sdkObject->freeManifest(manifestMem); };
    std::unique_ptr<const char, decltype(deleter)> manifestStr(
        sdkObject->manifest(&error),
        deleter);

    if (error != nx::sdk::Error::noError)
        return std::nullopt;

    return manifest<ManifestType>(manifestStr.get());
}

template<typename Interface, typename SdkObject>
Interface* queryInterface(SdkObject sdkObject, const nxpl::NX_GUID& guid)
{
    return static_cast<Interface*>(sdkObject->queryInterface(guid));
}

analytics::SdkObjectPool* getSdkObjectPool(QnMediaServerModule* serverModule);

bool deviceInfoFromResource(
    const QnVirtualCameraResourcePtr& device,
    nx::sdk::DeviceInfo* outDeviceInfo);

std::unique_ptr<nx::plugins::SettingsHolder> toSettingsHolder(const QVariantMap& settings);

void saveManifestToFile(
    const nx::utils::log::Tag& logTag,
    const char* const manifest,
    const QString& fileDescription,
    const QString& pluginLibName,
    const QString& filenameExtraSuffix);

std::optional<nx::sdk::analytics::UncompressedVideoFrame::PixelFormat>
    pixelFormatFromEngineManifest(const nx::vms::api::analytics::EngineManifest& manifest);

} // namespace nx::mediaserver::sdk_support
