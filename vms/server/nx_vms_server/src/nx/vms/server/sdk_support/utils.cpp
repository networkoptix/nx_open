#include "utils.h"

#include <core/resource/camera_resource.h>

#include <media_server/media_server_module.h>

#include <nx/utils/log/log.h>
#include <nx/utils/file_system.h>

#include <nx/sdk/i_plugin_event.h>
#include <nx/sdk/analytics/helpers/pixel_format.h>
#include <nx/sdk/analytics/helpers/object_track_info.h>
#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/vms/server/resource/resource_fwd.h>

#include <nx/vms_server_plugins/utils/uuid.h>

#include <nx/sdk/helpers/list.h>
#include <nx/sdk/analytics/helpers/timestamped_object_metadata.h>
#include <nx/vms/server/analytics/frame_converter.h>

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

nx::vms::api::analytics::PixelFormat fromSdkPixelFormat(
    IUncompressedVideoFrame::PixelFormat sdkPixelFormat)
{
    using namespace nx::vms::api::analytics;

    switch (sdkPixelFormat)
    {
        case IUncompressedVideoFrame::PixelFormat::yuv420:
            return PixelFormat::yuv420;
        case IUncompressedVideoFrame::PixelFormat::argb:
            return PixelFormat::argb;
        case IUncompressedVideoFrame::PixelFormat::abgr:
            return PixelFormat::abgr;
        case IUncompressedVideoFrame::PixelFormat::rgba:
            return PixelFormat::rgba;
        case IUncompressedVideoFrame::PixelFormat::bgra:
            return PixelFormat::bgra;
        case IUncompressedVideoFrame::PixelFormat::rgb:
            return PixelFormat::rgb;
        case IUncompressedVideoFrame::PixelFormat::bgr:
            return PixelFormat::bgr;
        default:
            NX_ASSERT(false, lm("Wrong pixel format, %1").args((int) sdkPixelFormat));
            return PixelFormat::undefined;
    }
}

std::optional<IUncompressedVideoFrame::PixelFormat> toSdkPixelFormat(
    nx::vms::api::analytics::PixelFormat pixelFormat)
{
    using namespace nx::vms::api::analytics;

    switch (pixelFormat)
    {
        case PixelFormat::yuv420:
            return IUncompressedVideoFrame::PixelFormat::yuv420;
        case PixelFormat::argb:
            return IUncompressedVideoFrame::PixelFormat::argb;
        case PixelFormat::abgr:
            return IUncompressedVideoFrame::PixelFormat::abgr;
        case PixelFormat::rgba:
            return IUncompressedVideoFrame::PixelFormat::rgba;
        case PixelFormat::bgra:
            return IUncompressedVideoFrame::PixelFormat::bgra;
        case PixelFormat::rgb:
            return IUncompressedVideoFrame::PixelFormat::rgb;
        case PixelFormat::bgr:
            return IUncompressedVideoFrame::PixelFormat::bgr;
        default:
            NX_ASSERT(false, lm("Wrong pixel format").args((int) pixelFormat));
            return std::nullopt;
    }
}

AVPixelFormat sdkToAvPixelFormat(IUncompressedVideoFrame::PixelFormat pixelFormat)
{
    using PixelFormat = IUncompressedVideoFrame::PixelFormat;
    switch (pixelFormat)
    {
        case PixelFormat::yuv420: return AV_PIX_FMT_YUV420P;
        case PixelFormat::argb: return AV_PIX_FMT_ARGB;
        case PixelFormat::abgr: return AV_PIX_FMT_ABGR;
        case PixelFormat::rgba: return AV_PIX_FMT_RGBA;
        case PixelFormat::bgra: return AV_PIX_FMT_BGRA;
        case PixelFormat::rgb: return AV_PIX_FMT_RGB24;
        case PixelFormat::bgr: return AV_PIX_FMT_BGR24;

        default:
            NX_ASSERT(false, lm("Unsupported PixelFormat value: %1").arg(
                pixelFormatToStdString(pixelFormat)));
            return AV_PIX_FMT_NONE;
    }
}

std::optional<nx::sdk::analytics::IUncompressedVideoFrame::PixelFormat> avPixelFormatToSdk(
    AVPixelFormat avPixelFormat)
{
    using PixelFormat = IUncompressedVideoFrame::PixelFormat;
    switch (avPixelFormat)
    {
        case AV_PIX_FMT_YUV420P: return PixelFormat::yuv420;
        case AV_PIX_FMT_ARGB: return PixelFormat::argb;
        case AV_PIX_FMT_ABGR: return PixelFormat::abgr;
        case AV_PIX_FMT_RGBA: return PixelFormat::rgba;
        case AV_PIX_FMT_BGRA: return PixelFormat::bgra;
        case AV_PIX_FMT_RGB24: return PixelFormat::rgb;
        case AV_PIX_FMT_BGR24: return PixelFormat::bgr;
        default: return std::nullopt;
    }
}

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

std::unique_ptr<nx::plugins::SettingsHolder> toSettingsHolder(const QVariantMap& settings)
{
    QMap<QString, QString> settingsMap;
    for (auto itr = settings.cbegin(); itr != settings.cend(); ++itr)
        settingsMap[itr.key()] = itr.value().toString();

    return std::make_unique<nx::plugins::SettingsHolder>(settingsMap);
}

Ptr<IStringMap> toIStringMap(const QVariantMap& map)
{
    const auto stringMap = makePtr<StringMap>();
    for (auto it = map.cbegin(); it != map.cend(); ++it)
        stringMap->addItem(it.key().toStdString(), it.value().toString().toStdString());

    return stringMap;
}

Ptr<IStringMap> toIStringMap(const QMap<QString, QString>& map)
{
    const auto stringMap = makePtr<StringMap>();
    for (auto it = map.cbegin(); it != map.cend(); ++it)
        stringMap->addItem(it.key().toStdString(), it.value().toStdString());

    return stringMap;
}

Ptr<IStringMap> toIStringMap(const QString& mapJson)
{
    bool isValid = false;
    const auto deserialized = QJson::deserialized<std::vector<StringMapItem>>(
        mapJson.toUtf8(), /*defaultValue*/ {}, &isValid);

    if (!isValid)
        return nullptr;

    const auto stringMap = makePtr<StringMap>();
    for (const auto& setting: deserialized)
    {
        if (stringMap->value(setting.name.c_str()) != nullptr) //< Duplicate key.
            return nullptr;
        stringMap->addItem(setting.name, setting.value);
    }

    return stringMap;
}

QVariantMap fromIStringMap(const IStringMap* map)
{
    QVariantMap variantMap;
    if (!map)
        return variantMap;

    const auto count = map->count();
    for (auto i = 0; i < count; ++i)
        variantMap.insert(map->key(i), map->value(i));

    return variantMap;
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

nx::vms::api::EventLevel fromSdkPluginEventLevel(IPluginEvent::Level level)
{
    using namespace nx::sdk;
    using namespace nx::vms::api;

    switch (level)
    {
        case IPluginEvent::Level::info:
            return EventLevel::InfoEventLevel;
        case IPluginEvent::Level::warning:
            return EventLevel::WarningEventLevel;
        case IPluginEvent::Level::error:
            return EventLevel::ErrorEventLevel;
        default:
            NX_ASSERT(false, "Wrong plugin event level");
            return EventLevel::UndefinedEventLevel;
    }
}

nx::sdk::Ptr<ITimestampedObjectMetadata> createTimestampedObjectMetadata(
    const nx::analytics::storage::DetectedObject& detectedObject,
    const nx::analytics::storage::ObjectPosition& objectPosition)
{
    auto objectMetadata = nx::sdk::makePtr<TimestampedObjectMetadata>();
    objectMetadata->setId(
        nx::vms_server_plugins::utils::fromQnUuidToSdkUuid(detectedObject.objectAppearanceId));
    objectMetadata->setTypeId(detectedObject.objectTypeId.toStdString());
    objectMetadata->setTimestampUs(objectPosition.timestampUsec);
    const auto& boundingBox = objectPosition.boundingBox;
    objectMetadata->setBoundingBox(Rect(
        boundingBox.x(),
        boundingBox.y(),
        boundingBox.width(),
        boundingBox.height()));

    for (const auto& attribute: detectedObject.attributes)
    {
        Attribute sdkAttribute(
            // Information about attribute types isn't stored in the database.
            nx::sdk::IAttribute::Type::undefined,
            attribute.name.toStdString(),
            attribute.value.toStdString());

        objectMetadata->addAttribute(std::move(sdkAttribute));
    }

    return objectMetadata;
}

nx::sdk::Ptr<nx::sdk::IList<ITimestampedObjectMetadata>> createObjectTrack(
    const nx::analytics::storage::DetectedObject& detectedObject)
{
    auto track = nx::sdk::makePtr<nx::sdk::List<ITimestampedObjectMetadata>>();
    for (const auto& objectPosition : detectedObject.track)
    {
        if (auto objectMetadataPtr =
            createTimestampedObjectMetadata(detectedObject, objectPosition))
        {
            track->addItem(objectMetadataPtr.get());
        }
    }

    return track;
}

nx::sdk::Ptr<IUncompressedVideoFrame> createUncompressedVideoFrame(
    const CLVideoDecoderOutputPtr& frame,
    nx::vms::api::analytics::PixelFormat pixelFormat)
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

    const auto dataPacket = frameConverter.getDataPacket(sdkPixelFormat);

    if (!dataPacket)
        return nullptr;

    return nx::sdk::queryInterfacePtr<IUncompressedVideoFrame>(dataPacket);
}

} // namespace nx::vms::server::sdk_support
