#include "engine.h"

#include <string>

#include <QtCore/QString>
#include <QtCore/QFileInfo>
#include <QtCore/QFile>

#include <nx/network/http/http_client.h>
#include <nx/fusion/model_functions.h>
#include <nx/kit/debug.h>

#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/error.h>

#include "device_agent.h"

namespace nx::vms_server_plugins::analytics::axis {

namespace {

const QString kAxisVendor("axis");

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

void Engine::setEngineInfo(const nx::sdk::analytics::IEngineInfo* /*engineInfo*/)
{
}

void Engine::doSetSettings(
    Result<const IStringMap*>* /*outResult*/, const IStringMap* /*settings*/)
{
    // There are no DeviceAgent settings for this plugin.
}

void Engine::getPluginSideSettings(Result<const ISettingsResponse*>* /*outResult*/) const
{
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    if (!isCompatible(deviceInfo))
    {
        *outResult = error(ErrorCode::invalidParams, "Device is not compatible");
        return;
    }

    EngineManifest events = fetchSupportedEvents(deviceInfo);
    if (events.eventTypes.empty())
    {
        *outResult = error(ErrorCode::internalError, "Supported event list is empty");
        return;
    }

    *outResult = new DeviceAgent(this, deviceInfo, events);
}

void Engine::getManifest(Result<const IString*>* outResult) const
{
    *outResult = new nx::sdk::String(m_jsonManifest);
}

EngineManifest Engine::fetchSupportedEvents(const IDeviceInfo* deviceInfo)
{
    EngineManifest result;
    const char* const ip_port = deviceInfo->url() + sizeof("http://") - 1;
    nx::axis::CameraController axisCameraController(
        ip_port,
        deviceInfo->login(),
        deviceInfo->password());
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
        [](const nx::axis::SupportedEventType& eventType) { return EventType(eventType); });

    // Being uncommented, the next code line allows to get the list of supported events in the same
    // json format that is used in "static-resources/manifest.json". All you need is to stop on a
    // breakpoint and copy the text form the debugger.
    //QString textForManifest = QJson::serialized(result);

    return result;
}

void Engine::doExecuteAction(Result<IAction::Result>* /*outResult*/, const IAction* /*action*/)
{
}

void Engine::setHandler(IHandler* /*handler*/)
{
    // TODO: Use the handler for error reporting.
}

bool Engine::isCompatible(const IDeviceInfo* deviceInfo) const
{
    const auto vendor = QString(deviceInfo->vendor()).toLower();
    return vendor.startsWith(kAxisVendor);
}

} // nx::vms_server_plugins::analytics::axis

namespace {

static const std::string kPluginManifest = /*suppress newline*/ 1 + R"json(
{
    "id": "nx.axis",
    "name": "Axis analytics plugin",
    "description": "Supports built-in analytics on Axis cameras",
    "version": "1.0.0",
    "engineSettingsModel": ""
}
)json";

} // namespace

extern "C" {

NX_PLUGIN_API nx::sdk::IPlugin* createNxPlugin()
{
    return new nx::sdk::analytics::Plugin(
        kPluginManifest,
        [](nx::sdk::analytics::IPlugin* plugin)
        {
            return new nx::vms_server_plugins::analytics::axis::Engine(
                dynamic_cast<nx::sdk::analytics::Plugin*>(plugin));
        });
}

} // extern "C"
