#include "engine.h"

#include <array>
#include <fstream>
#include <string>

#include <QtCore/QString>
#include <QtCore/QUrlQuery>
#include <QtCore/QFileInfo>
#include <QtCore/QFile>

#include <nx/network/http/http_client.h>
#include <nx/fusion/model_functions.h>
#include <nx/mediaserver_plugins/utils/uuid.h>
#include <nx/kit/debug.h>

#include "device_agent.h"

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace axis {

namespace {

const QString kAxisVendor("axis");
const QString kSoapPath("/vapix/services");

} // namespace

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(CommonPlugin* plugin): m_plugin(plugin)
{
    QFile f(":/axis/manifest.json");
    if (f.open(QFile::ReadOnly))
        m_manifest = f.readAll();

    {
        QFile file("plugins/axis/manifest.json");
        if (file.open(QFile::ReadOnly))
        {
            NX_PRINT << "Switch to external manifest file "
                << QFileInfo(file).absoluteFilePath().toStdString();
            m_manifest = file.readAll();
        }
    }
    m_typedManifest = QJson::deserialized<EngineManifest>(m_manifest);
}

void* Engine::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_Engine)
    {
        addRef();
        return static_cast<Engine*>(this);
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
    // There are no DeviceAgent settings for this plugin.
}

nx::sdk::Settings* Engine::settings() const
{
    return nullptr;
}

nx::sdk::analytics::DeviceAgent* Engine::obtainDeviceAgent(
    const DeviceInfo* deviceInfo,
    Error* outError)
{
    *outError = Error::noError;

    const auto vendor = QString(deviceInfo->vendor).toLower();
    if (!vendor.startsWith(kAxisVendor))
        return nullptr;

    EngineManifest events = fetchSupportedEvents(*deviceInfo);
    if (events.eventTypes.empty())
        return nullptr;

    return new DeviceAgent(this, *deviceInfo, events);
}

const char* Engine::manifest(Error* error) const
{
    *error = Error::noError;
    return m_manifest.constData();
}

void Engine::freeManifest(const char* manifestData)
{
    // Do nothing actually.
}

EngineManifest Engine::fetchSupportedEvents(const DeviceInfo& deviceInfo)
{
    EngineManifest result;
    const char* const ip_port = deviceInfo.url + sizeof("http://") - 1;
    nx::axis::CameraController axisCameraController(ip_port, deviceInfo.login,
        deviceInfo.password);
    if (!axisCameraController.readSupportedEvents())
        return result;

    // Only some rules are useful.
    std::vector<std::string> allowedTopics;
    for (const auto& topic: m_typedManifest.allowedTopics)
        allowedTopics.push_back(topic.toStdString());
    axisCameraController.filterSupportedEvents(allowedTopics);

    std::vector<std::string> forbiddenDescriptions;
    for (const auto& description: m_typedManifest.forbiddenDescriptions)
        forbiddenDescriptions.push_back(description.toStdString());
    axisCameraController.removeForbiddenEvents(forbiddenDescriptions);

    const auto& src = axisCameraController.suppotedEvents();
    std::transform(src.begin(), src.end(), std::back_inserter(result.eventTypes),
        [](const nx::axis::SupportedEventType& eventType) {return EventType(eventType); });

    // Being uncommented, the next code line allows to get the list of supported events in the same
    // json format that is used in "static-resources/manifest.json". All you need is to stop on a
    // breakpoint and copy the text form the debugger.
    //QString textForManifest = QJson::serialized(result);

    return result;
}

void Engine::executeAction(Action* /*action*/, Error* /*outError*/)
{
    // Do nothing.
}

} // namespace axis
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx

namespace {

static const std::string kPluginName = "Axis driver";
static const std::string kPluginManifest = R"json(
{
    "id": "nx.axis",
    "name": ")json" + kPluginName + R"json(",
    "version": "1.0.0",
    "engineSettingsModel": "",
    "deviceAgentSettingsModel": ""
})json";

} // namespace

extern "C" {

NX_PLUGIN_API nxpl::PluginInterface* createNxAnalyticsPlugin()
{
    return new nx::sdk::analytics::CommonPlugin(
        kPluginName,
        kPluginManifest,
        [](nx::sdk::analytics::Plugin* plugin)
        {
            return new nx::mediaserver_plugins::analytics::axis::Engine(
                dynamic_cast<nx::sdk::analytics::CommonPlugin*>(plugin));
        });
}

} // extern "C"
