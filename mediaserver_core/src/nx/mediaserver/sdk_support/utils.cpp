#include "utils.h"

#include <QtCore/QFileInfo>
#include <QtCore/QDir>

#include <core/resource/camera_resource.h>

#include <media_server/media_server_module.h>3

#include <nx/utils/log/log.h>
#include <nx/utils/file_system.h>

#include <plugins/plugins_ini.h>

namespace nx::mediaserver::sdk_support {

namespace {

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


analytics::SdkObjectPool* getSdkObjectPool(QnMediaServerModule* serverModule)
{
    if (!serverModule)
    {
        NX_ASSERT(false, "Can't access server module");
        return nullptr;
    }

    auto sdkObjectPool = serverModule->sdkObjectPool();
    if (!sdkObjectPool)
    {
        NX_ASSERT(false, "Can't access SDK object pool");
        return nullptr;
    }

    return sdkObjectPool;
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

} // namespace nx::mediaserver::sdk_support
