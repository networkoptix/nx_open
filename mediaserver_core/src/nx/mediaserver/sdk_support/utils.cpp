#include "utils.h"

#include <QtCore/QFileInfo>
#include <QtCore/QDir>

#include <core/resource/camera_resource.h>

#include <media_server/media_server_module.h>

#include <nx/utils/log/log.h>
#include <nx/utils/file_system.h>

#include <plugins/plugins_ini.h>

#include <nx/analytics/descriptor_list_manager.h>
#include <nx/sdk/analytics/pixel_format.h>
#include <nx/sdk/common_settings.h>
#include <nx/mediaserver/resource/resource_fwd.h>

namespace nx::mediaserver::sdk_support {

namespace {

nx::utils::log::Tag kLogTag(QString("SdkSupportUtils"));

/** @return Dir ending with "/", intended to receive manifest files. */
static QString manifestFileDir()
{
    QString dir = QDir::cleanPath( //< Normalize to use forward slashes, as required by QFile.
        QString::fromUtf8(pluginsIni().analyticsManifestOutputPath));

    if (QFileInfo(dir).isRelative())
    {
        dir.insert(0,
            // NOTE: QDir::cleanPath() removes trailing '/'.
            QDir::cleanPath(QString::fromUtf8(nx::kit::IniConfig::iniFilesDir())) + lit("/"));
    }

    if (!dir.isEmpty() && dir.at(dir.size() - 1) != '/')
        dir.append('/');

    return dir;
}

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

nx::analytics::DescriptorListManager* getDescriptorListManager(QnMediaServerModule* serverModule)
{
    if (!serverModule)
    {
        NX_ASSERT(false, "Can't access server module");
        return nullptr;
    }

    auto commonModule = serverModule->commonModule();
    if (!commonModule)
    {
        NX_ASSERT(false, "Can't access common module");
        return nullptr;
    }

    auto descriptorListManager = commonModule->analyticsDescriptorListManager();
    if (!descriptorListManager)
    {
        NX_ASSERT(false, "Can't access descriptor list manager");
        return nullptr;
    }

    return descriptorListManager;
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
        CameraInfo::kStringParameterMaxLength);

    strncpy(
        outDeviceInfo->model,
        device->getModel().toUtf8().data(),
        CameraInfo::kStringParameterMaxLength);

    strncpy(
        outDeviceInfo->firmware,
        device->getFirmware().toUtf8().data(),
        CameraInfo::kStringParameterMaxLength);

    strncpy(
        outDeviceInfo->uid,
        device->getId().toByteArray().data(),
        CameraInfo::kStringParameterMaxLength);

    strncpy(
        outDeviceInfo->sharedId,
        device->getSharedId().toStdString().c_str(),
        CameraInfo::kStringParameterMaxLength);

    strncpy(
        outDeviceInfo->url,
        device->getUrl().toUtf8().data(),
        CameraInfo::kTextParameterMaxLength);

    auto auth = device->getAuth();
    strncpy(
        outDeviceInfo->login,
        auth.user().toUtf8().data(),
        CameraInfo::kStringParameterMaxLength);

    strncpy(
        outDeviceInfo->password,
        auth.password().toUtf8().data(),
        CameraInfo::kStringParameterMaxLength);

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

UniquePtr<nx::sdk::Settings> toSdkSettings(const QVariantMap& settings)
{
    auto sdkSettings = new nx::sdk::CommonSettings();
    for (auto itr = settings.cbegin(); itr != settings.cend(); ++itr)
        sdkSettings->addSetting(itr.key().toStdString(), itr.value().toString().toStdString());

    return UniquePtr<nx::sdk::Settings>(sdkSettings);
}

UniquePtr<nx::sdk::Settings> toSdkSettings(const QMap<QString, QString>& settings)
{
    auto sdkSettings = new nx::sdk::CommonSettings();
    for (auto itr = settings.cbegin(); itr != settings.cend(); ++itr)
        sdkSettings->addSetting(itr.key().toStdString(), itr.value().toStdString());

    return UniquePtr<nx::sdk::Settings>(sdkSettings);
}

QVariantMap fromSdkSettings(const nx::sdk::Settings* sdkSettings)
{
    QVariantMap result;
    if (!sdkSettings)
        return result;

    const auto count = sdkSettings->count();
    for (auto i = 0; i < count; ++i)
        result.insert(sdkSettings->key(i), sdkSettings->value(i));

    return result;
}

void saveManifestToFile(
    const nx::utils::log::Tag& logTag,
    const char* const manifest,
    const QString& fileDescription,
    const QString& pluginLibName,
    const QString& filenameExtraSuffix)
{
    const QString dir = manifestFileDir();
    const QString filename = dir + pluginLibName + filenameExtraSuffix + lit("_manifest.json");

    using nx::utils::log::Level;
    auto log = //< Can be used to return after logging: return log(...).
        [&](Level level, const QString& message)
        {
            NX_UTILS_LOG(level, logTag) << lm("Metadata %1 manifest: %2: [%3]")
                .args(fileDescription, message, filename);
        };

    log(Level::info, lit("Saving to file"));

    if (!nx::utils::file_system::ensureDir(dir))
        return log(Level::error, lit("Unable to create directory for file"));

    QFile f(filename);
    if (!f.open(QFile::WriteOnly))
        return log(Level::error, lit("Unable to (re)create file"));

    const qint64 len = (qint64)strlen(manifest);
    if (f.write(manifest, len) != len)
        return log(Level::error, lit("Unable to write to file"));
}

std::optional<nx::sdk::analytics::UncompressedVideoFrame::PixelFormat>
    pixelFormatFromEngineManifest(const nx::vms::api::analytics::EngineManifest& manifest)
{
    using PixelFormat = nx::sdk::analytics::UncompressedVideoFrame::PixelFormat;
    using Capability = nx::vms::api::analytics::EngineManifest::Capability;

    int uncompressedFrameCapabilityCount = 0; //< To check there is 0 or 1 of such capabilities.
    PixelFormat pixelFormat = PixelFormat::yuv420;

    // To assert that all pixel formats are tested.
    auto pixelFormats = nx::sdk::analytics::getAllPixelFormats();

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
            "in Engine manifest of analytics plugin \"%1\"").arg(manifest.pluginId);
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

} // namespace nx::mediaserver::sdk_support
