#include "utils.h"

#include <QtCore/QFileInfo>
#include <QtCore/QDir>

#include <core/resource/camera_resource.h>

#include <media_server/media_server_module.h>

#include <nx/utils/log/log.h>
#include <nx/utils/file_system.h>

#include <plugins/plugins_ini.h>

#include <nx/sdk/analytics/common/pixel_format.h>
#include <nx/sdk/common/ptr.h>
#include <nx/sdk/common/string_map.h>
#include <nx/vms/server/resource/resource_fwd.h>

#include <nx/fusion/model_functions.h>

namespace nx::vms::server::sdk_support {

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

bool deviceInfoFromResource(
    const QnVirtualCameraResourcePtr& device,
    nx::sdk::DeviceInfo* outDeviceInfo)
{
    using namespace nx::sdk;

    if (!outDeviceInfo)
    {
        NX_ASSERT(false, "Device info is invalid");
        return false;
    }

    if (!device)
    {
        NX_ASSERT(false, "Device is empty");
        return false;
    }

    strncpy(
        outDeviceInfo->vendor,
        device->getVendor().toUtf8().data(),
        DeviceInfo::kStringParameterMaxLength);

    strncpy(
        outDeviceInfo->model,
        device->getModel().toUtf8().data(),
        DeviceInfo::kStringParameterMaxLength);

    strncpy(
        outDeviceInfo->firmware,
        device->getFirmware().toUtf8().data(),
        DeviceInfo::kStringParameterMaxLength);

    strncpy(
        outDeviceInfo->uid,
        device->getId().toByteArray().data(),
        DeviceInfo::kStringParameterMaxLength);

    strncpy(
        outDeviceInfo->sharedId,
        device->getSharedId().toStdString().c_str(),
        DeviceInfo::kStringParameterMaxLength);

    strncpy(
        outDeviceInfo->url,
        device->getUrl().toUtf8().data(),
        DeviceInfo::kTextParameterMaxLength);

    auto auth = device->getAuth();
    strncpy(
        outDeviceInfo->login,
        auth.user().toUtf8().data(),
        DeviceInfo::kStringParameterMaxLength);

    strncpy(
        outDeviceInfo->password,
        auth.password().toUtf8().data(),
        DeviceInfo::kStringParameterMaxLength);

    outDeviceInfo->channel = device->getChannel();
    outDeviceInfo->logicalId = device->logicalId();

    return true;
}

std::unique_ptr<nx::plugins::SettingsHolder> toSettingsHolder(const QVariantMap& settings)
{
    QMap<QString, QString> settingsMap;
    for (auto itr = settings.cbegin(); itr != settings.cend(); ++itr)
        settingsMap[itr.key()] = itr.value().toString();

    return std::make_unique<nx::plugins::SettingsHolder>(settingsMap);
}

nx::sdk::common::Ptr<nx::sdk::IStringMap> toIStringMap(const QVariantMap& map)
{
    auto stringMap = new nx::sdk::common::StringMap();
    for (auto it = map.cbegin(); it != map.cend(); ++it)
        stringMap->addItem(it.key().toStdString(), it.value().toString().toStdString());

    return nx::sdk::common::Ptr<nx::sdk::IStringMap>(stringMap);
}

nx::sdk::common::Ptr<nx::sdk::IStringMap> toIStringMap(const QMap<QString, QString>& map)
{
    auto stringMap = new nx::sdk::common::StringMap();
    for (auto it = map.cbegin(); it != map.cend(); ++it)
        stringMap->addItem(it.key().toStdString(), it.value().toStdString());

    return nx::sdk::common::Ptr<nx::sdk::IStringMap>(stringMap);
}

nx::sdk::common::Ptr<nx::sdk::IStringMap> toIStringMap(const QString& mapJson)
{
    bool isValid = false;
    const auto deserialized = QJson::deserialized<std::vector<StringMapItem>>(
        mapJson.toUtf8(), /*defaultValue*/ {}, &isValid);

    if (!isValid)
        return nullptr;

    auto stringMap = new nx::sdk::common::StringMap();
    for (const auto& setting: deserialized)
    {
        if (stringMap->value(setting.name.c_str()) != nullptr) //< Duplicate key.
            return nullptr;
        stringMap->addItem(setting.name, setting.value);
    }

    return nx::sdk::common::Ptr<nx::sdk::IStringMap>(stringMap);
}

QVariantMap fromIStringMap(const nx::sdk::IStringMap* map)
{
    QVariantMap variantMap;
    if (!map)
        return variantMap;

    const auto count = map->count();
    for (auto i = 0; i < count; ++i)
        variantMap.insert(map->key(i), map->value(i));

    return variantMap;
}

std::optional<nx::sdk::analytics::IUncompressedVideoFrame::PixelFormat>
    pixelFormatFromEngineManifest(
        const nx::vms::api::analytics::EngineManifest& manifest,
        const QString& engineLogLabel)
{
    using PixelFormat = nx::sdk::analytics::IUncompressedVideoFrame::PixelFormat;
    using Capability = nx::vms::api::analytics::EngineManifest::Capability;

    int uncompressedFrameCapabilityCount = 0; //< To check there is 0 or 1 of such capabilities.
    PixelFormat pixelFormat = PixelFormat::yuv420;

    // To assert that all pixel formats are tested.
    auto pixelFormats = nx::sdk::analytics::common::getAllPixelFormats();

    auto checkCapability =
        [&](Capability value, PixelFormat correspondingPixelFormat)
        {
            if (manifest.capabilities.testFlag(value))
            {
                ++uncompressedFrameCapabilityCount;
                pixelFormat = correspondingPixelFormat;
            }

            // Delete the pixel format which has been tested.
            auto it = std::find(pixelFormats.begin(), pixelFormats.end(), correspondingPixelFormat);
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

resource::AnalyticsEngineResourceList toServerEngineList(
    const nx::vms::common::AnalyticsEngineResourceList engineList)
{
    resource::AnalyticsEngineResourceList result;
    for (const auto& engine: engineList)
    {
        auto serverEngine = engine.dynamicCast<resource::AnalyticsEngineResource>();
        if (serverEngine)
            result.push_back(serverEngine);
    }

    return result;
}

nx::vms::api::EventLevel fromSdkPluginEventLevel(nx::sdk::IPluginEvent::Level level)
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

} // namespace nx::vms::server::sdk_support
