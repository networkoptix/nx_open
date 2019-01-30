#include "engine.h"

#include <fstream>

extern "C" {

#include <gst/gst.h>

} // extern "C"

#include <nx/sdk/helpers/ptr.h>

#define NX_PRINT_PREFIX "deepstream::Engine::"
#include <nx/kit/debug.h>

#include <nx/gstreamer/gstreamer_common.h>
#include <nx/sdk/helpers/string.h>

#include "device_agent.h"
#include "utils.h"
#include "deepstream_common.h"
#include "openalpr_common.h"
#include "deepstream_analytics_plugin_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(Plugin* plugin): m_plugin(plugin)
{
    if (strcmp(ini().debugDotFilesDir, "") != 0)
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

    m_timeUtilityProvider = queryInterfacePtr<ITimeUtilityProvider>(m_plugin->utilityProvider());
    NX_KIT_ASSERT(m_timeUtilityProvider);
}

Engine::~Engine()
{
    NX_PRINT << " Destroyed Engine for " << m_plugin->name();
}

void Engine::setSettings(const IStringMap* settings)
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

IStringMap* Engine::pluginSideSettings() const
{
    return nullptr;
}

const IString* Engine::manifest(Error* error) const
{
    *error = Error::noError;

    if (!m_manifest.empty())
        return new nx::sdk::String(m_manifest);

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
        for (int i = 0; i < (int) m_objectClassDescritions.size(); ++i)
        {
            objectTypesManifest += buildManifestObectTypeString(m_objectClassDescritions[i]);
            if (i < (int) m_objectClassDescritions.size() - 1)
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

    return new nx::sdk::String(m_manifest);
}

IDeviceAgent* Engine::obtainDeviceAgent(
    const IDeviceInfo* deviceInfo, Error* outError)
{
    NX_OUTPUT
        << __func__
        << " Obtaining DeviceAgent for the device "
        << deviceInfo->vendor() << ", "
        << deviceInfo->model() << ", "
        << deviceInfo->id();

    *outError = Error::noError;

    // Deepstream can't be correctly deinitialized, so we never destroy the DeviceAgent.
    // It's not a production-ready solution, but is OK for demos.
    if (!m_deviceAgent)
        m_deviceAgent = new DeviceAgent(this, deviceInfo->id());

    m_deviceAgent->addRef();
    return m_deviceAgent;
}

void Engine::executeAction(IAction* /*action*/, Error* /*outError*/)
{
}

std::vector<ObjectClassDescription> Engine::objectClassDescritions() const
{
    return m_objectClassDescritions;
}

std::chrono::microseconds Engine::currentTimeUs() const
{
    return std::chrono::microseconds(m_timeUtilityProvider->vmsSystemTimeSinceEpochMs() * 1000);
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
    for (int i = 0; i < (int) labels.size(); ++i)
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

Error Engine::setHandler(IHandler* /*handler*/)
{
    // TODO: Implement.
    return Error::noError;
}

bool Engine::isCompatible(const IDeviceInfo* /*deviceInfo*/) const
{
    return true;
}

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

namespace {

static const std::string kLibName = "deepstream_analytics_plugin";

static const std::string kPluginManifest = /*suppress newline*/1 + R"json(
{
    "id": "nx.deepstream",
    "name": "DeepStream analytics plugin",
    "engineSettingsModel": ""
}
)json";

} // namespace

extern "C" {

NX_PLUGIN_API nx::sdk::IPlugin* createNxPlugin()
{
    return new nx::sdk::analytics::Plugin(
        kLibName,
        kPluginManifest,
        [](nx::sdk::analytics::IPlugin* plugin)
        {
            return new nx::vms_server_plugins::analytics::deepstream::Engine(
                dynamic_cast<nx::sdk::analytics::Plugin*>(plugin));
        });
}

} // extern "C"
