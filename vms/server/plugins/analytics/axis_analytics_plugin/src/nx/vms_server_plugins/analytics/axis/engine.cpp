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
#include <nx/kit/debug.h>

#include <nx/sdk/helpers/string.h>

#include "device_agent.h"

namespace nx::vms_server_plugins::analytics::axis {
namespace {

const QString kAxisVendor("axis");
const QString kSoapPath("/vapix/services");

} // namespace

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(Plugin* plugin): m_plugin(plugin)
{
    QFile f(":/axis/manifest.json");
    if (f.open(QFile::ReadOnly))
        m_jsonManifest = f.readAll();

    {
        QFile file("plugins/axis/manifest.json");
        if (file.open(QFile::ReadOnly))
        {
            NX_PRINT << "Switch to external manifest file "
                << QFileInfo(file).absoluteFilePath().toStdString();
            m_jsonManifest = file.readAll();
        }
    }
    m_parsedManifest = QJson::deserialized<EngineManifest>(m_jsonManifest);
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

void Engine::setSettings(const IStringMap* /*settings*/)
{
    // There are no DeviceAgent settings for this plugin.
}

IStringMap* Engine::pluginSideSettings() const
{
    return nullptr;
}

IDeviceAgent* Engine::obtainDeviceAgent(
    const DeviceInfo* deviceInfo,
    Error* outError)
{
    *outError = Error::noError;

    if (!isCompatible(deviceInfo))
        return nullptr;

    EngineManifest events = fetchSupportedEvents(*deviceInfo);

    if (events.eventTypes.empty())
        return nullptr;

    return new DeviceAgent(this, *deviceInfo, events);
}

const IString* Engine::manifest(Error* error) const
{
    *error = Error::noError;
    return new nx::sdk::String(m_jsonManifest);
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
    for (const auto& topic: m_parsedManifest.allowedTopics)
        allowedTopics.push_back(topic.toStdString());
    axisCameraController.filterSupportedEvents(allowedTopics);

    std::vector<std::string> forbiddenDescriptions;
    for (const auto& description: m_parsedManifest.forbiddenDescriptions)
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

void Engine::executeAction(IAction* /*action*/, Error* /*outError*/)
{
}

Error Engine::setHandler(IHandler* /*handler*/)
{
    // TODO: Use the handler for error reporting.
    return Error::noError;
}

bool Engine::isCompatible(const DeviceInfo* deviceInfo) const
{
    const auto vendor = QString(deviceInfo->vendor).toLower();
    return vendor.startsWith(kAxisVendor);
}

} // nx::vms_server_plugins::analytics::axis

namespace {

static const std::string kLibName = "axis_analytics_plugin";
static const std::string kPluginManifest = /*suppress newline*/ 1 + R"json(
{
    "id": "nx.axis",
    "name": "Axis analytics plugin",
    "engineSettingsModel": ""
}
)json";

} // namespace

extern "C" {

NX_PLUGIN_API nxpl::PluginInterface* createNxAnalyticsPlugin()
{
    return new nx::sdk::analytics::Plugin(
        kLibName,
        kPluginManifest,
        [](nx::sdk::analytics::IPlugin* plugin)
        {
            return new nx::vms_server_plugins::analytics::axis::Engine(
                dynamic_cast<nx::sdk::analytics::Plugin*>(plugin));
        });
}

} // extern "C"
