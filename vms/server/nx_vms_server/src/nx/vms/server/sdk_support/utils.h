#pragma once

#include <QtCore/QVariantMap>

#include <core/resource/resource_fwd.h>
#include <core/resource_management/resource_pool.h>

#include <media_server/media_server_module.h>

#include <nx/utils/std/optional.h>
#include <nx/utils/log/log_level.h>
#include <nx/utils/member_detector.h>

#include <nx/sdk/analytics/i_uncompressed_video_frame.h>

#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/descriptors.h>

#include <nx/vms/server/resource/resource_fwd.h>
#include <nx/vms/server/resource/analytics_plugin_resource.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>

#include <nx/vms/server/sdk_support/loggers.h>

#include <nx/sdk/i_string_map.h>
#include <nx/sdk/i_plugin_event.h>
#include <plugins/settings.h>

class QnMediaServerModule;

namespace nx::vms::server::analytics {

class SdkObjectFactory;

} // namespace nx::vms::server::analytics

namespace nx::vms::server::sdk_support {

namespace detail {

NX_UTILS_DECLARE_FIELD_DETECTOR(hasGroupId, groupId, std::set<QString>);
NX_UTILS_DECLARE_FIELD_DETECTOR(hasPaths, paths,
    std::set<nx::vms::api::analytics::DescriptorScope>);
NX_UTILS_DECLARE_FIELD_DETECTOR_SIMPLE(hasItem, item);

} // namespace detail

template<typename ManifestType, typename SdkObjectPtr>
std::optional<ManifestType> manifest(
    const SdkObjectPtr& sdkObject,
    std::unique_ptr<AbstractManifestLogger> logger = nullptr)
{
    nx::sdk::Error error = nx::sdk::Error::noError;
    const nx::sdk::Ptr<const nx::sdk::IString> manifestStr(sdkObject->manifest(&error));

    auto log =
        [&logger](
            const QString& manifestString,
            nx::sdk::Error error,
            const QString& customError = QString())
        {
            if (logger)
                logger->log(manifestString, error, customError);
        };

    if (error != nx::sdk::Error::noError)
    {
        log(QString(), error);
        return std::nullopt;
    }

    if (!manifestStr)
    {
        log(QString(), nx::sdk::Error::unknownError, "No manifest string");
        return std::nullopt;
    }

    const auto rawString = manifestStr->str();
    if (!NX_ASSERT(rawString))
        return std::nullopt;

    bool success = false;
    auto deserializedManifest = QJson::deserialized(rawString, ManifestType(), &success);
    if (!success)
    {
        log(rawString, nx::sdk::Error::unknownError, "Can't deserialize manifest");
        return std::nullopt;
    }

    log(rawString, nx::sdk::Error::noError);
    return deserializedManifest;
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

bool deviceInfoFromResource(
    const QnVirtualCameraResourcePtr& device,
    nx::sdk::DeviceInfo* outDeviceInfo);

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

resource::AnalyticsEngineResourceList toServerEngineList(
    const nx::vms::common::AnalyticsEngineResourceList engineList);

nx::vms::api::EventLevel fromSdkPluginEventLevel(nx::sdk::IPluginEvent::Level level);

} // namespace nx::vms::server::sdk_support
