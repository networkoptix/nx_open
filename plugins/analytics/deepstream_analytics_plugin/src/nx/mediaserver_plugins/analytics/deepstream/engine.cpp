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

void Engine::setSettings(const nx::sdk::Settings* settings)
{
    NX_OUTPUT << __func__ << " Received " << m_plugin->name() << " settings:";
    NX_OUTPUT << "{";

    const auto count = settings->count();
    for (int i = 0; i < count; ++i)
    {
        NX_OUTPUT << "    " << settings->key(i)
            << ": " << settings->value(i)
            << ((i < count - 1) ? "," : "");
    }
    NX_OUTPUT << "}";
}

nx::sdk::Settings* Engine::settings() const
{
    return nullptr;
}

const char* Engine::manifest(Error* error) const
{
    *error = Error::noError;

    if (!m_manifest.empty())
        return m_manifest.c_str();

    std::string objectTypesManifest;
    if (ini().pipelineType == kOpenAlprPipeline)
    {
        ObjectClassDescription licensePlateDescription(
            "License Plate",
            "",
            kLicensePlateGuid);

        objectTypesManifest += buildManifestObectTypeString(licensePlateDescription);
    }
    else
    {
        for (auto i = 0; i < m_objectClassDescritions.size(); ++i)
        {
            objectTypesManifest += buildManifestObectTypeString(m_objectClassDescritions[i]);
            if (i < m_objectClassDescritions.size() - 1)
                objectTypesManifest += ",\n";
        }
    }

    m_manifest = /*suppress newline*/1 + R"json(
{
    "pluginId": "nx.deepstream",
    "pluginName": {
        "value": "DeepStream Driver",
        "localization": {
            "ru_RU": "DeepStream driver (translated to Russian)"
        }
    },
    "objectTypes": [
)json" + objectTypesManifest + R"json(
    ],
    "capabilities": ""
}
)json";

    return m_manifest.c_str();
}

void Engine::freeManifest(const char* /*manifestData*/)
{
    // Do nothing.
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

/** Use indentation of 8 spaces, and no trailing whitespace. */
std::string Engine::buildManifestObectTypeString(const ObjectClassDescription& description) const
{
    return /*suppress newline*/1 + R"json(
        {
            "id": ")json" + description.typeId + R"json(",
            "name": {
                "value": ")json" + description.name + R"json(",
                "localization": {}
            }
        })json";
}

} // namespace deepstream
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx

namespace {

static const std::string kLibName = "deepstream_analytics_plugin";
static const std::string kPluginManifest = R"json(
{
    "id": "nx.deepstream",
    "name": "DeepStream analytics plugin",
    "version": "1.0.0",
    "engineSettingsModel": "",
    "deviceAgentSettingsModel": ""
})json";

} // namespace

extern "C" {

NX_PLUGIN_API nxpl::PluginInterface* createNxAnalyticsPlugin()
{
    return new nx::sdk::analytics::CommonPlugin(
        kLibName,
        kPluginManifest,
        [](nx::sdk::analytics::Plugin* plugin)
        {
            return new nx::mediaserver_plugins::analytics::deepstream::Engine(
                dynamic_cast<nx::sdk::analytics::CommonPlugin*>(plugin));
        });
}

} // extern "C"
