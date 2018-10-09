#include "engine.h"

#include <fstream>

extern "C" {

#include <gst/gst.h>

} // extern "C"

#include "device_agent.h"
#include "utils.h"
#include "deepstream_common.h"
#include "openalpr_common.h"
#include "deepstream_analytics_plugin_ini.h"
#define NX_PRINT_PREFIX "deepstream::Engine::"
#include <nx/kit/debug.h>
#include <nx/gstreamer/gstreamer_common.h>

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace deepstream {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(CommonPlugin* plugin): m_plugin(plugin)
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
    NX_PRINT << " Created Engine for " << plugin->name();

    if (m_objectClassDescritions.empty())
        m_objectClassDescritions = loadObjectClasses();

    NX_OUTPUT << __func__ << " Setting timeProvider";
    nxpt::ScopedRef<nxpl::TimeProvider> timeProvider(
        m_plugin->pluginContainer()->queryInterface(nxpl::IID_TimeProvider));

    if (timeProvider)
    {
        m_timeProvider = decltype(m_timeProvider)(
            timeProvider.release(),
            [](nxpl::TimeProvider* provider) { provider->releaseRef(); });
    }
}

Engine::~Engine()
{
    NX_PRINT << " Destroyed Engine for " << m_plugin->name();
}

void* Engine::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == nx::sdk::analytics::IID_Engine)
    {
        addRef();
        return static_cast<nx::sdk::analytics::Engine*>(this);
    }
    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

void Engine::setSettings(const nxpl::Setting *settings, int count)
{
    NX_OUTPUT << __func__ << " Received " << m_plugin->name() << " settings:";
    NX_OUTPUT << "{";
    for (int i = 0; i < count; ++i)
    {
        NX_OUTPUT << "    " << settings[i].name
            << ": " << settings[i].value
            << ((i < count - 1) ? "," : "");
    }
    NX_OUTPUT << "}";
}


const char* Engine::manifest(Error* error) const
{
    *error = Error::noError;

    if (!m_manifest.empty())
        return m_manifest.c_str();

    static const std::string kManifestPrefix = R"json(
        {
            "pluginId": "nx.deepstream",
            "pluginName": {
                "value": "DeepStream Driver",
                "localization": {
                    "ru_RU": "DeepStream driver (translated to Russian)"
                }
            },
            "outputObjectTypes": [
        )json";

    static const std::string kManifestPostfix = R"json(
            ],
            "capabilities": ""
        })json";

    m_manifest = kManifestPrefix;

    if (ini().pipelineType == kOpenAlprPipeline)
    {
        ObjectClassDescription licensePlateDescription(
            "License Plate",
            "",
            kLicensePlateGuid);

        m_manifest += buildManifestObectTypeString(licensePlateDescription);
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

nx::sdk::analytics::DeviceAgent* Engine::obtainDeviceAgent(
    const DeviceInfo* deviceInfo, Error* outError)
{
    NX_OUTPUT
        << __func__
        << " Obtaining DeviceAgent for the device "
        << deviceInfo->vendor << ", "
        << deviceInfo->model << ", "
        << deviceInfo->uid;

    *outError = Error::noError;
    return new DeviceAgent(this, std::string(deviceInfo->uid));
}

void Engine::executeAction(nx::sdk::analytics::Action*, nx::sdk::Error*)
{
    // Do nothing.
}

std::vector<ObjectClassDescription> Engine::objectClassDescritions() const
{
    return m_objectClassDescritions;
}

std::chrono::microseconds Engine::currentTimeUs() const
{
    return std::chrono::microseconds(m_timeProvider->millisSinceEpoch() * 1000);
}

std::vector<ObjectClassDescription> Engine::loadObjectClasses() const
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
        result.emplace_back(/*name*/ labels[i], /*description*/ "", /*typeId*/ guids[i]);

    return result;
}

std::vector<std::string> Engine::loadLabels(const std::string& labelFilePath) const
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

std::vector<std::string> Engine::loadClassGuids(const std::string& guidsFilePath) const
{
    std::vector<std::string> result;
    std::ifstream guidsFile(guidsFilePath, std::ios::in);

    std::string line;
    while (std::getline(guidsFile, line))
    {
        if (trim(&line)->empty())
            continue;

        if (line.empty())
            continue;

        result.push_back(line);
    }

    return result;
}

std::string Engine::buildManifestObectTypeString(const ObjectClassDescription& description) const
{
    static const std::string kObjectTypeStringPrefix = (R"json({
        "id": ")json");
    static const std::string kObjectTypeStringMiddle = (R"json(",
        "name": {
            "value": ")json");
    static const std::string kObjectTypeStringPostfix = (R"json(",
            "localization": {}
        }
    })json");

    return kObjectTypeStringPrefix
        + description.typeId
        + kObjectTypeStringMiddle
        + description.name
        + kObjectTypeStringPostfix;
}

} // namespace deepstream
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx

extern "C" {

NX_PLUGIN_API nxpl::PluginInterface* createNxAnalyticsPlugin()
{
    return new nx::sdk::analytics::CommonPlugin("deepstream_analytics_plugin",
        [](nx::sdk::analytics::Plugin* plugin)
        {
            return new nx::mediaserver_plugins::analytics::deepstream::Engine(
                dynamic_cast<nx::sdk::analytics::CommonPlugin*>(plugin));
        });
}

} // extern "C"
