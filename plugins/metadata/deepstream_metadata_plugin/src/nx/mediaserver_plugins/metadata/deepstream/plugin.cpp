#include "plugin.h"

extern "C" {

#include <gst/gst.h>

} // extern "C"

#include "manager.h"
#include "deepstream_common.h"
#include "deepstream_metadata_plugin_ini.h"

#define NX_PRINT_PREFIX "metadata::deepstream::Plugin::"
#include <nx/kit/debug.h>
#include <nx/gstreamer/gstreamer_common.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

Plugin::Plugin()
{
    if (strcmp(ini().debugDotFilesDir, "") == 0)
    {
        NX_PRINT
            << __func__
            << " Setting " << nx::gstreamer::kGstreamerDebugDumpDotDirEnv
            << " environment variable to " << ini().debugDotFilesDir;

        setenv(nx::gstreamer::kGstreamerDebugDumpDotDirEnv, ini().debugDotFilesDir, 1);
    }

    auto gstreamerDebugLevel = ini().gstreamerDebugLevel;
    if (gstreamerDebugLevel > nx::gstreamer::kMaxGstreamerDebugLevel)
        gstreamerDebugLevel = nx::gstreamer::kMaxGstreamerDebugLevel;

    if (gstreamerDebugLevel < nx::gstreamer::kMinGstreamerDebugLevel)
        gstreamerDebugLevel = nx::gstreamer::kMinGstreamerDebugLevel;

    NX_PRINT << __func__ << " Setting GStreamer debug level to " << gstreamerDebugLevel;
    setenv(
        nx::gstreamer::kGstreamerDebugLevelEnv,
        std::to_string(gstreamerDebugLevel).c_str(), 1);

    gst_init(NULL, NULL);
    NX_PRINT << " Created plugin: \"" << name() << "\"";
}

Plugin::~Plugin()
{
    NX_PRINT << " Destroyed plugin: \"" << name() << "\"";
}

void* Plugin::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == nx::sdk::metadata::IID_Plugin)
    {
        addRef();
        return static_cast<nx::sdk::metadata::Plugin*>(this);
    }

    if (interfaceId == nxpl::IID_Plugin3)
    {
        addRef();
        return static_cast<nxpl::Plugin3*>(this);
    }

    if (interfaceId == nxpl::IID_Plugin2)
    {
        addRef();
        return static_cast<nxpl::Plugin2*>(this);
    }

    if (interfaceId == nxpl::IID_Plugin)
    {
        addRef();
        return static_cast<nxpl::Plugin*>(this);
    }

    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

void Plugin::setDeclaredSettings(const nxpl::Setting *settings, int count)
{
    NX_OUTPUT << __func__ << " Received " << name() << " declared settings:";
    NX_OUTPUT << "{";
    for (int i = 0; i < count; ++i)
    {
        NX_OUTPUT << "    " << settings[i].name
            << ": " << settings[i].value
            << ((i < count - 1) ? "," : "");
    }
    NX_OUTPUT << "}";
}

const char* Plugin::name() const
{
    return "DeepStream metadata plugin";
}

void Plugin::setSettings(const nxpl::Setting* settings, int count)
{
    NX_OUTPUT << __func__ << " Received " << name() << " settings:";
    NX_OUTPUT << "{";
    for (int i = 0; i < count; ++i)
    {
        NX_OUTPUT << "    " << settings[i].name
            << ": " << settings[i].value
            << ((i < count - 1) ? "," : "");
    }
    NX_OUTPUT << "}";
}

void Plugin::setPluginContainer(nxpl::PluginInterface* /*pluginContainer*/)
{
    NX_OUTPUT << __func__ << " Setting plugin container";
}

void Plugin::setLocale(const char* locale)
{
    NX_OUTPUT << __func__ << " Setting locale to " << locale;
}

const char* Plugin::capabilitiesManifest(Error* error) const
{
    *error = Error::noError;

    static const auto pluginManifest = (R"json(
        {
            "driverId": "{2CAE7F92-01E5-478D-ABA4-50938034D46E}",
            "driverName": {
                "value": "DeepStream Driver",
                "localization": {
                    "ru_RU": "DeepStream driver (translated to Russian)"
                }
            },
            "outputObjectTypes": [
                {
                    "typeId": ")json" + nxpt::NxGuidHelper::toStdString(kCarGuid) + R"json(",
                    "name": {
                        "value": "Car",
                        "localization": {
                            "ru_RU": "Car detected (translated to Russian)"
                        }
                    }
                },
                {
                    "typeId": ")json" + nxpt::NxGuidHelper::toStdString(kHumanGuid) + R"json(",
                    "name": {
                        "value": "Human",
                        "localization": {
                            "ru_RU": "Human (translated to Russian)"
                        }
                    }
                },
                {
                    "typeId": ")json" + nxpt::NxGuidHelper::toStdString(kRcGuid) + R"json(",
                    "name": {
                        "value": "RC",
                        "localization": {
                            "ru_RU": "RC (chto eto? translated to Russian)"
                        }
                    }
                }
            ],
            "capabilities": "needDeepCopyForMediaFrame"
        }
    )json");

    return pluginManifest.c_str();
}

CameraManager *Plugin::obtainCameraManager(
    const CameraInfo &cameraInfo, Error *outError)
{
    NX_OUTPUT
        << __func__
        << " Obtaining camera manager for the camera "
        << cameraInfo.vendor << ", "
        << cameraInfo.model << ", "
        << cameraInfo.uid;

    *outError = Error::noError;
    return new Manager(this, std::string(cameraInfo.uid));
}

} // namespace deepstream
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

extern "C" {

NX_PLUGIN_API nxpl::PluginInterface* createNxMetadataPlugin()
{
    return new nx::mediaserver_plugins::metadata::deepstream::Plugin();
}

} // extern "C"
