#pragma once

#include <QtCore/QVariantMap>

#include <core/resource/resource_fwd.h>
#include <core/resource_management/resource_pool.h>

#include <media_server/media_server_module.h>

#include <nx/utils/std/optional.h>
#include <nx/utils/log/log_level.h>
#include <nx/utils/meta/member_detector.h>

#include <nx/sdk/common.h>
#include <nx/sdk/analytics/uncompressed_video_frame.h>

#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/descriptors.h>

#include <nx/mediaserver/resource/resource_fwd.h>
#include <nx/mediaserver/resource/analytics_plugin_resource.h>
#include <nx/mediaserver/resource/analytics_engine_resource.h>

#include <nx/mediaserver/sdk_support/loggers.h>

#include <nx/sdk/settings.h>
#include <nx/sdk/i_plugin_event.h>
#include <plugins/settings.h>

class QnMediaServerModule;

namespace nx::analytics {

class DescriptorListManager;

} // namespace nx::analytics

namespace nx::mediaserver::analytics {

class SdkObjectFactory;

} // namespace nx::mediaserver::analytics

namespace nx::mediaserver::sdk_support {

namespace details {

DECLARE_FIELD_DETECTOR(hasGroupId, groupId, std::set<QString>);
DECLARE_FIELD_DETECTOR(hasPaths, paths, std::set<nx::vms::api::analytics::HierarchyPath>);
DECLARE_FIELD_DETECTOR_SIMPLE(hasItem, item);

} // namespace details

template<typename ManifestType, typename SdkObjectPtr>
std::optional<ManifestType> manifest(
    const SdkObjectPtr& sdkObject,
    std::unique_ptr<AbstractManifestLogger> logger = nullptr)
{
    nx::sdk::Error error = nx::sdk::Error::noError;
    sdk_support::UniquePtr<const nx::sdk::IString> manifestStr(sdkObject->manifest(&error));

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
        return QnSharedResourcePointer<ResourceType>();
    }

    auto resourcePool = serverModule->resourcePool();
    if (!resourcePool)
    {
        NX_ASSERT(false, "Can't access resource pool");
        return QnSharedResourcePointer<ResourceType>();
    }

    return resourcePool->getResourceById<ResourceType>(QnUuid(id));
}

analytics::SdkObjectFactory* getSdkObjectFactory(QnMediaServerModule* serverModule);
nx::analytics::DescriptorListManager* getDescriptorListManager(QnMediaServerModule* serverModule);

bool deviceInfoFromResource(
    const QnVirtualCameraResourcePtr& device,
    nx::sdk::DeviceInfo* outDeviceInfo);

std::unique_ptr<nx::plugins::SettingsHolder> toSettingsHolder(const QVariantMap& settings);

UniquePtr<nx::sdk::Settings> toSdkSettings(const QVariantMap& settings);
UniquePtr<nx::sdk::Settings> toSdkSettings(const QMap<QString, QString>& settings);
UniquePtr<nx::sdk::Settings> toSdkSettings(const QString& settingsJson);

QVariantMap fromSdkSettings(const nx::sdk::Settings* sdkSettings);

void saveManifestToFile(
    const nx::utils::log::Tag& logTag,
    const QString& manifest,
    const QString& baseFileName);

std::optional<nx::sdk::analytics::UncompressedVideoFrame::PixelFormat>
    pixelFormatFromEngineManifest(
        const nx::vms::api::analytics::EngineManifest& manifest,
        const QString& engineLogLabel);

resource::AnalyticsEngineResourceList toServerEngineList(
    const nx::vms::common::AnalyticsEngineResourceList engineList);

template <typename Descriptor, typename Item>
std::map<QString, Descriptor> descriptorsFromItemList(
    const QString& pluginId,
    const QList<Item>& itemList)
{
    std::map<QString, Descriptor> result;
    for (const auto& item: itemList)
    {
        Descriptor descriptor;
        if constexpr(details::hasItem<Descriptor>::value)
        {
            descriptor.item = item;
        }
        else
        {
            descriptor.id = item.id;
            descriptor.name = item.name.value;
        }

        if constexpr (details::hasPaths<Descriptor>::value)
        {
            nx::vms::api::analytics::HierarchyPath path;
            path.pluginId = pluginId;

            if constexpr (details::hasGroupId<Item>::value)
                path.groupId = item.groupId;

            descriptor.paths.insert(path);
        }

        result[item.id] = std::move(descriptor);
    }

    return result;
}

nx::vms::api::EventLevel fromSdkPluginEventLevel(nx::sdk::IPluginEvent::Level level);

} // namespace nx::mediaserver::sdk_support
