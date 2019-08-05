#include "file_utils.h"

#include <nx/utils/log/assert.h>

#include <core/resource/camera_resource.h>
#include <nx/vms/server/resource/analytics_plugin_resource.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <media_server/media_server_module.h>
#include <plugins/plugin_manager.h>

namespace nx::vms::server::sdk_support {

// Examples of filenames used to save/load manifests:
// DeviceAgent: stub_analytics_plugin_device_a25c01be-f7dc-4600-8b8e-915bf4c0a688_manifest.json
// Engine: stub_analytics_plugin_engine_manifest.json
// Plugin: stub_analytics_plugin_manifest.json
//
// Examples of filenames used to save/load settings:
// DeviceAgent: stub_analytics_plugin_device_a25c01be-f7dc-4600-8b8e-915bf4c0a688_settings.json
// Engine: stub_analytics_plugin_engine_settings.json

namespace {

static QString nameOfFileToDumpOrLoadDataForDevice(
    const QnVirtualCameraResourcePtr& deviceResource,
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engineResource,
    bool isDeviceSpecific)
{
    const QString deviceStr = engineResource->libName() + "_device";
    if (!NX_ASSERT(engineResource))
        return deviceStr;

    if (!NX_ASSERT(deviceResource))
        return deviceStr + "_missingDeviceResource";

    return deviceStr + (isDeviceSpecific
        ? "_" + deviceResource->getId().toSimpleString()
        : QString());
}

static QString nameOfFileToDumpOrLoadDataForEngine(
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engineResource,
    bool isEngineSpecific)
{
    const QString engineStr = engineResource->libName() + "_engine";

    if (!NX_ASSERT(engineResource))
        return engineStr;

    const QString engineResourceIdSuffix =
        isEngineSpecific ? ("_" + engineResource->getId().toSimpleString()) : "";

    return engineStr + engineResourceIdSuffix;
}

} // namespace

std::optional<QString> loadStringFromFile(
    const nx::utils::log::Tag& logTag,
    const QString& absoluteFilePath)
{
    if (!NX_ASSERT(!absoluteFilePath.isEmpty()))
    {
        NX_DEBUG(logTag, "Unable to load string from file: filename is empty");
        return std::nullopt;
    }

    if (!QFileInfo::exists(absoluteFilePath))
    {
        NX_DEBUG(logTag,
            "Unable to load string from file: file %1 doesn't exist", absoluteFilePath);
        return std::nullopt;
    }

    QFile f(absoluteFilePath);
    if (!f.open(QFile::ReadOnly))
    {
        NX_DEBUG(logTag, "Unable to load string file: unable to open %1", absoluteFilePath);
        return std::nullopt;
    }

    return f.readAll();
}

bool dumpStringToFile(
    const nx::utils::log::Tag& logTag,
    const QString& absoluteFilePath,
    const QString& stringToDump)
{
    if (!NX_ASSERT(!absoluteFilePath.isEmpty()))
    {
        NX_DEBUG(logTag, "Unable to dump string to file: filename is empty");
        return false;
    }

    QFileInfo fileInfo(absoluteFilePath);
    if (!fileInfo.absoluteDir().exists())
    {
        NX_DEBUG(logTag,
            "Unable to dump string to file: parent directory of %1 doesn't exist",
            absoluteFilePath);
        return false;
    }

    QFile f(absoluteFilePath);
    if (!f.open(QFile::WriteOnly))
    {
        NX_DEBUG(logTag,
            "Unable to dump string to file: unable to open %1");
        return false;
    }


    if (f.write(stringToDump.toUtf8()) < 0)
    {
        NX_DEBUG(logTag,
            "Unable to dump string to file: error occurred while writing to %1", absoluteFilePath);
        return false;
    }

    if (!stringToDump.isEmpty() && stringToDump.back() != '\n')
    {
        if (f.write("\n") < 0)
        {
            NX_DEBUG(logTag,
                "Unable to dump string to file: unable to write trailing end-of-line symbol");
            return false;
        }
    }

    return true;
}

QString debugFileAbsolutePath(
    const QString& debugDirectoryPath,
    const QString filenameWithoutPath)
{
    if (!NX_ASSERT(!debugDirectoryPath.isEmpty()))
        return QString();

    if (!NX_ASSERT(!filenameWithoutPath.isEmpty()))
        return QString();

    if (QDir::isAbsolutePath(debugDirectoryPath))
        return QDir(debugDirectoryPath).absoluteFilePath(filenameWithoutPath);

    return QDir(QString(nx::kit::IniConfig::iniFilesDir()) + debugDirectoryPath)
        .absoluteFilePath(filenameWithoutPath);
}

QString baseNameOfFileToDumpOrLoadData(
    const resource::AnalyticsPluginResourcePtr& plugin,
    const resource::AnalyticsEngineResourcePtr& engine,
    const QnVirtualCameraResourcePtr& device,
    bool isEngineSpecific,
    bool isDeviceSpecific)
{
    if (device && engine) //< Filename for something related to a DeviceAgent.
        return nameOfFileToDumpOrLoadDataForDevice(device, engine, isDeviceSpecific);
    else if (engine) //< Filename for something related to an Engine.
        return nameOfFileToDumpOrLoadDataForEngine(engine, isEngineSpecific);
    else if (plugin) //< Filename for something related to a Plugin.
        return plugin->libName();

    NX_ASSERT(false, "Incorrect parameters");
    return QString();
}

} // namespace nx::vms::server::sdk_support
