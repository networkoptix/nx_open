#include "utils.h"

#include <core/resource/camera_resource.h>

#include <media_server/media_server_module.h>

#include <nx/utils/log/log.h>
#include <nx/utils/file_system.h>

#include <nx/sdk/i_plugin_diagnostic_event.h>
#include <nx/sdk/i_attribute.h>
#include <nx/sdk/analytics/helpers/pixel_format.h>
#include <nx/sdk/analytics/helpers/object_track_info.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/to_string.h>
#include <nx/vms/server/resource/resource_fwd.h>

#include <nx/vms_server_plugins/utils/uuid.h>

#include <nx/sdk/helpers/list.h>
#include <nx/sdk/analytics/helpers/timestamped_object_metadata.h>
#include <nx/vms/server/analytics/frame_converter.h>
#include <nx/vms/server/sdk_support/conversion_utils.h>

#include <nx/fusion/model_functions.h>

namespace nx::vms::server::sdk_support {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace {

struct StringMapItem
{
    std::string name;
    std::string value;
};
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(StringMapItem, (json), (name)(value), (optional, false));

nx::utils::log::Tag kLogTag(QString("SdkSupportUtils"));

} // namespace


analytics::SdkObjectFactory* getSdkObjectFactory(QnMediaServerModule* serverModule)
{
    if (!serverModule)
    {
        NX_ASSERT(false, "Can't access server module");
        return nullptr;
    }

    auto sdkObjectFactory = serverModule->sdkObjectFactory();
    if (!sdkObjectFactory)
    {
        NX_ASSERT(false, "Can't access SDK object pool");
        return nullptr;
    }

    return sdkObjectFactory;
}

Ptr<DeviceInfo> deviceInfoFromResource(const QnVirtualCameraResourcePtr& device)
{
    using namespace nx::sdk;

    if (!NX_ASSERT(device, "No device has been provided"))
        return nullptr;

    auto deviceInfo = makePtr<DeviceInfo>();

    deviceInfo->setId(device->getId().toStdString());
    deviceInfo->setName(device->getUserDefinedName().toStdString());
    deviceInfo->setVendor(device->getVendor().toStdString());
    deviceInfo->setModel(device->getModel().toStdString());
    deviceInfo->setFirmware(device->getFirmware().toStdString());
    deviceInfo->setSharedId(device->getSharedId().toStdString());
    deviceInfo->setLogicalId(QString::number(device->logicalId()).toStdString());
    deviceInfo->setUrl(device->getUrl().toStdString());

    auto auth = device->getAuth();
    deviceInfo->setLogin(auth.user().toStdString());
    deviceInfo->setPassword(auth.password().toStdString());

    deviceInfo->setChannelNumber(device->getChannel());

    return deviceInfo;
}

std::optional<QJsonObject> toQJsonObject(const QString& mapJson)
{
    bool isValid = false;
    const auto& result = QJson::deserialized<QJsonObject>(
        mapJson.toUtf8(), /*defaultValue*/{}, &isValid);

    if (!isValid)
        return std::nullopt;

    return std::move(result);
}

std::optional<IUncompressedVideoFrame::PixelFormat>
    pixelFormatFromEngineManifest(
        const nx::vms::api::analytics::EngineManifest& manifest,
        const QString& engineLogLabel)
{
    using PixelFormat = IUncompressedVideoFrame::PixelFormat;
    using Capability = nx::vms::api::analytics::EngineManifest::Capability;

    int uncompressedFrameCapabilityCount = 0; //< To check there is 0 or 1 of such capabilities.
    PixelFormat pixelFormat = PixelFormat::yuv420;

    // To assert that all pixel formats are tested.
    auto pixelFormats = getAllPixelFormats();

    auto checkCapability =
        [&](Capability value, PixelFormat correspondingPixelFormat)
        {
            if (manifest.capabilities.testFlag(value))
            {
                ++uncompressedFrameCapabilityCount;
                pixelFormat = correspondingPixelFormat;
            }

            // Delete the pixel format which has been tested.
            auto it = std::find(pixelFormats.begin(), pixelFormats.end(),
                correspondingPixelFormat);
            NX_ASSERT(it != pixelFormats.end());
            pixelFormats.erase(it);
        };

    checkCapability(Capability::needUncompressedVideoFrames_yuv420, PixelFormat::yuv420);
    checkCapability(Capability::needUncompressedVideoFrames_argb, PixelFormat::argb);
    checkCapability(Capability::needUncompressedVideoFrames_abgr, PixelFormat::abgr);
    checkCapability(Capability::needUncompressedVideoFrames_rgba, PixelFormat::rgba);
    checkCapability(Capability::needUncompressedVideoFrames_bgra, PixelFormat::bgra);
    checkCapability(Capability::needUncompressedVideoFrames_rgb, PixelFormat::rgb);
    checkCapability(Capability::needUncompressedVideoFrames_bgr, PixelFormat::bgr);

    NX_ASSERT(pixelFormats.empty());

    NX_ASSERT(uncompressedFrameCapabilityCount >= 0);
    if (uncompressedFrameCapabilityCount > 1)
    {
        NX_ERROR(kLogTag) << lm(
            "More than one needUncompressedVideoFrames_... capability found"
            "in %1").arg(engineLogLabel);
    }
    if (uncompressedFrameCapabilityCount != 1)
        return std::nullopt;

    return pixelFormat;
}

nx::vms::api::EventLevel fromPluginDiagnosticEventLevel(IPluginDiagnosticEvent::Level level)
{
    using namespace nx::sdk;
    using namespace nx::vms::api;

    switch (level)
    {
        case IPluginDiagnosticEvent::Level::info:
            return EventLevel::InfoEventLevel;
        case IPluginDiagnosticEvent::Level::warning:
            return EventLevel::WarningEventLevel;
        case IPluginDiagnosticEvent::Level::error:
            return EventLevel::ErrorEventLevel;
        default:
            NX_ASSERT(false, "Wrong plugin event level");
            return EventLevel::UndefinedEventLevel;
    }
}

nx::sdk::Ptr<ITimestampedObjectMetadata> createTimestampedObjectMetadata(
    const nx::analytics::db::ObjectTrack& track,
    const nx::analytics::db::ObjectPosition& objectPosition)
{
    auto objectMetadata = nx::sdk::makePtr<TimestampedObjectMetadata>();
    objectMetadata->setTrackId(
        nx::vms_server_plugins::utils::fromQnUuidToSdkUuid(track.id));
    objectMetadata->setTypeId(track.objectTypeId.toStdString());
    objectMetadata->setTimestampUs(objectPosition.timestampUs);
    const auto& boundingBox = objectPosition.boundingBox;
    objectMetadata->setBoundingBox(Rect(
        boundingBox.x(),
        boundingBox.y(),
        boundingBox.width(),
        boundingBox.height()));

    for (const auto& attribute: track.attributes)
    {
        auto sdkAttribute = nx::sdk::makePtr<Attribute>(
            // Information about attribute types isn't stored in the database.
            nx::sdk::IAttribute::Type::undefined,
            attribute.name.toStdString(),
            attribute.value.toStdString());

        objectMetadata->addAttribute(std::move(sdkAttribute));
    }

    return objectMetadata;
}

nx::sdk::Ptr<nx::sdk::IList<ITimestampedObjectMetadata>> createObjectTrack(
    const nx::analytics::db::ObjectTrackEx& track)
{
    auto timestampedTrack = nx::sdk::makePtr<nx::sdk::List<ITimestampedObjectMetadata>>();
    for (const auto& objectPosition: track.objectPositionSequence)
    {
        if (auto objectMetadataPtr = createTimestampedObjectMetadata(track, objectPosition))
            timestampedTrack->addItem(objectMetadataPtr.get());
    }

    return timestampedTrack;
}

nx::sdk::Ptr<IUncompressedVideoFrame> createUncompressedVideoFrame(
    const CLVideoDecoderOutputPtr& frame,
    nx::vms::api::analytics::PixelFormat pixelFormat,
    int rotationAngle)
{
    bool compressedFrameWarningIssued = false;
    nx::vms::server::analytics::FrameConverter frameConverter(
        /*compressedFrame*/ nullptr,
        frame,
        &compressedFrameWarningIssued);

    const std::optional<IUncompressedVideoFrame::PixelFormat> sdkPixelFormat =
        toSdkPixelFormat(pixelFormat);
    if (!NX_ASSERT(sdkPixelFormat, lm("Wrong pixel format %1").args((int) pixelFormat)))
        return nullptr;

    const auto dataPacket = frameConverter.getDataPacket(sdkPixelFormat, rotationAngle);

    if (!dataPacket)
        return nullptr;

    return dataPacket->queryInterface<IUncompressedVideoFrame>();
}

std::map<QString, QString> attributesMap(
    const nx::sdk::Ptr<const nx::sdk::analytics::IMetadata>& metadata)
{
    if (!NX_ASSERT(metadata, "Metadata mustn't be null"))
        return {};

    std::map<QString, QString> result;
    for (int i = 0; i < metadata->attributeCount(); ++i)
    {
        const auto attribute = metadata->attribute(i);
        result.emplace(
            QString::fromStdString(attribute->name()),
            QString::fromStdString(attribute->value()));
    }

    return result;
}

} // namespace nx::vms::server::sdk_support
