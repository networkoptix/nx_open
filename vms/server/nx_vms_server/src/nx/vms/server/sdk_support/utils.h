#pragma once

#include <optional>

#include <QtCore/QJsonObject>

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

#include <nx/vms/server/sdk_support/result_holder.h>

#include <analytics/db/analytics_db_types.h>

#include <nx/sdk/i_string_map.h>
#include <nx/sdk/i_plugin_diagnostic_event.h>
#include <nx/sdk/result.h>

#include <nx/sdk/helpers/device_info.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/helpers/to_string.h>

#include <plugins/settings.h>
#include <plugins/vms_server_plugins_ini.h>

class QnMediaServerModule;

namespace nx::vms::server::analytics { class SdkObjectFactory; }

namespace nx::vms::server::sdk_support {

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

std::optional<QJsonObject> toQJsonObject(const QString& mapJson);

std::optional<nx::sdk::analytics::IUncompressedVideoFrame::PixelFormat>
    pixelFormatFromEngineManifest(
        const nx::vms::api::analytics::EngineManifest& manifest,
        const QString& engineLogLabel);

nx::vms::api::EventLevel fromPluginDiagnosticEventLevel(
    nx::sdk::IPluginDiagnosticEvent::Level level);

nx::sdk::Ptr<nx::sdk::analytics::ITimestampedObjectMetadata> createTimestampedObjectMetadata(
    const nx::analytics::db::ObjectTrack& track,
    const nx::analytics::db::ObjectPosition& objectPosition);

nx::sdk::Ptr<nx::sdk::IList<nx::sdk::analytics::ITimestampedObjectMetadata>> createObjectTrack(
    const nx::analytics::db::ObjectTrackEx& track);

nx::sdk::Ptr<nx::sdk::analytics::IUncompressedVideoFrame> createUncompressedVideoFrame(
    const CLVideoDecoderOutputPtr& frame,
    nx::vms::api::analytics::PixelFormat pixelFormat,
    int rotationAngle = 0);

std::map<QString, QString> attributesMap(
    const nx::sdk::Ptr<const nx::sdk::analytics::IMetadata>& metadata);

} // namespace nx::vms::server::sdk_support
