#include "plugin.h"

#include <fstream>

extern "C" {

#include <gst/gst.h>

} // extern "C"

#include "manager.h"
#include "utils.h"
#include "deepstream_common.h"
#include "openalpr_common.h"
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

    if (m_objectClassDescritions.empty())
        m_objectClassDescritions = loadObjectClasses();
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

void Plugin::setPluginContainer(nxpl::PluginInterface* pluginContainer)
{
    NX_OUTPUT << __func__ << " Setting plugin container";
    nxpt::ScopedRef<nxpl::TimeProvider> timeProvider(
        pluginContainer->queryInterface(nxpl::IID_TimeProvider));

    if (timeProvider)
    {
        m_timeProvider = decltype(m_timeProvider)(
            timeProvider.release(),
            [](nxpl::TimeProvider* provider) { provider->releaseRef(); });
    }
}

void Plugin::setLocale(const char* locale)
{
    NX_OUTPUT << __func__ << " Setting locale to " << locale;
}

const char* Plugin::capabilitiesManifest(Error* error) const
{
    *error = Error::noError;

    if (!m_manifest.empty())
        return m_manifest.c_str();

    static const std::string kManifestPrefix = (R"json(
        {
            "driverId": "{2CAE7F92-01E5-478D-ABA4-50938034D46E}",
            "driverName": {
                "value": "DeepStream Driver",
                "localization": {
                    "ru_RU": "DeepStream driver (translated to Russian)"
                }
            },
            "outputObjectTypes": [
        )json");

    static const std::string kManifestPostfix = (R"json(
            ],
            "capabilities": ""
        })json");

    m_manifest = kManifestPrefix;

    if (ini().pipelineType == kOpenAlprPipeline)
    {
        ObjectClassDescription licensePlateDescrition(
            "License Plate",
            "",
            kLicensePlateGuid);

        m_manifest += buildManifestObectTypeString(licensePlateDescrition);
    }
    else
    {
        for (auto i = 0; i < m_objectClassDescritions.size(); ++i)
        {
            m_manifest += buildManifestObectTypeString(m_objectClassDescritions[i]);
            if (i < m_objectClassDescritions.size() - 1)
                m_manifest += ',';
        }
    }

    m_manifest += kManifestPostfix;
    return m_manifest.c_str();
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

void Plugin::executeAction(nx::sdk::metadata::Action*, nx::sdk::Error*)
{
    // Do nothing.
}

std::vector<ObjectClassDescription> Plugin::objectClassDescritions() const
{
    return m_objectClassDescritions;
}

std::chrono::microseconds Plugin::currentTimeUs() const
{
    return std::chrono::microseconds(m_timeProvider->millisSinceEpoch() * 1000);
}

std::vector<ObjectClassDescription> Plugin::loadObjectClasses() const
{
    if (!ini().pgie_enabled)
        return {};

    const auto labelFile = ini().pgie_labelFile;
    if (labelFile[0] == '\0')
    {
        NX_OUTPUT << __func__ << " Error: label file is not specified";
        return {};
    }

    const auto classGuidsFile = ini().pgie_classGuidsFile;
    if (classGuidsFile[0] == '\0')
    {
        NX_OUTPUT << __func__ << " Error: class guids file is not specified";
        return {};
    }

    auto labels = loadLabels(labelFile);
    auto guids = loadClassGuids(classGuidsFile);

    if (labels.size() != guids.size())
    {
        NX_OUTPUT
            << __func__
            << " Error: number of labels is not equal to number of class guids: "
            << labels.size() <<  " labels, "
            << guids.size() << " guids";

        return {};
    }

    std::vector<ObjectClassDescription> result;
    for (auto i = 0; i < labels.size(); ++i)
        result.emplace_back(/*name*/ labels[i], /*description*/ "", /*guid*/ guids[i]);

    return result;
}

std::vector<std::string> Plugin::loadLabels(const std::string& labelFilePath) const
{
    std::vector<std::string> result;
    std::ifstream labelFile(labelFilePath, std::ios::in);

    std::string line;
    while (std::getline(labelFile, line))
    {
        if (trim(&line)->empty())
            continue;

        result.push_back(line);
    }

    return result;
}

std::vector<nxpl::NX_GUID> Plugin::loadClassGuids(const std::string& guidsFilePath) const
{
    std::vector<nxpl::NX_GUID> result;
    std::ifstream guidsFile(guidsFilePath, std::ios::in);

    std::string line;
    while (std::getline(guidsFile, line))
    {
        if (trim(&line)->empty())
            continue;

        auto guid = nxpt::fromStdString(line);
        if (guid == kNullGuid)
            continue;

        result.push_back(guid);
    }

    return result;
}

std::string Plugin::buildManifestObectTypeString(const ObjectClassDescription& description) const
{
    static const std::string kObjectTypeStringPrefix = (R"json({
        "typeId": ")json");
    static const std::string kObjectTypeStringMiddle = (R"json(",
        "name": {
            "value": ")json");
    static const std::string kObjectTypeStringPostfix = (R"json(",
            "localization": {}
        }
    })json");

    return kObjectTypeStringPrefix
        + nxpt::toStdString(description.guid)
        + kObjectTypeStringMiddle
        + description.name
        + kObjectTypeStringPostfix;
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
