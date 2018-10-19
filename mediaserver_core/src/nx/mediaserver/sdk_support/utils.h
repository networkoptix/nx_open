#pragma once

#include <QtCore/QVariantMap>

#include <core/resource/resource_fwd.h>

#include <nx/utils/std/optional.h>
#include <nx/utils/log/log_level.h>

#include <nx/sdk/common.h>
#include <nx/sdk/analytics/uncompressed_video_frame.h>

#include <nx/vms/api/analytics/engine_manifest.h>

#include <nx/mediaserver/resource/resource_fwd.h>
#include <nx/mediaserver/resource/analytics_plugin_resource.h>
#include <nx/mediaserver/resource/analytics_engine_resource.h>

#include <nx/sdk/settings.h>
#include <plugins/settings.h>

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
    nx::sdk::Error error = nx::sdk::Error::noError;

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

template<typename ResourceType>
QnSharedResourcePointer<ResourceType> find(QnMediaServerModule* serverModule, const QString& id)
{
    if (!serverModule)
    {
        NX_ASSERT(false, "Can't access server module");
        return nullptr;
    }

    auto resourcePool = serverModule->resourcePool();
    if (!resourcePool)
    {
        NX_ASSERT(false, "Can't access resource pool");
        return nullptr;
    }

    return resourcePool->getResourceById<ResourceType>(QnUuid(id));
}

analytics::SdkObjectPool* getSdkObjectPool(QnMediaServerModule* serverModule);

bool deviceInfoFromResource(
    const QnVirtualCameraResourcePtr& device,
    nx::sdk::DeviceInfo* outDeviceInfo);

std::unique_ptr<nx::plugins::SettingsHolder> toSettingsHolder(const QVariantMap& settings);

UniquePtr<nx::sdk::Settings> toSdkSettings(const QVariantMap& settings);
UniquePtr<nx::sdk::Settings> toSdkSettings(const QMap<QString, QString>& settings);

QVariantMap fromSdkSettings(const nx::sdk::Settings* sdkSettings);

void saveManifestToFile(
    const nx::utils::log::Tag& logTag,
    const char* const manifest,
    const QString& fileDescription,
    const QString& pluginLibName,
    const QString& filenameExtraSuffix);

std::optional<nx::sdk::analytics::UncompressedVideoFrame::PixelFormat>
    pixelFormatFromEngineManifest(const nx::vms::api::analytics::EngineManifest& manifest);

resource::AnalyticsEngineResourceList toServerEngineList(
    const nx::vms::common::AnalyticsEngineResourceList engineList);

} // namespace nx::mediaserver::sdk_support
