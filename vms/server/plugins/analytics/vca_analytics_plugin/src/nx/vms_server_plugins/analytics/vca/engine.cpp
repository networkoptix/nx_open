#include "engine.h"

#include <QtCore/QString>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <nx/fusion/model_functions.h>

#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/error.h>

#include "device_agent.h"
#include "log.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace vca {

namespace {

static const QString kVcaVendor("cap");

} // namespace

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(Plugin* plugin): m_plugin(plugin)
{
    static const char* const kResourceName = ":/vca/manifest.json";
    static const char* const kFileName = "plugins/vca/manifest.json";

    QFile f(kResourceName);
    if (f.open(QFile::ReadOnly))
        m_manifest = f.readAll();
    {
        QFile file(kFileName);
        if (file.open(QFile::ReadOnly))
        {

            NX_PRINT << "Switch to external manifest file "
                << QFileInfo(file).absoluteFilePath().toStdString();

            m_manifest = file.readAll();
        }
    }
    m_typedManifest = QJson::deserialized<EngineManifest>(m_manifest);
}

void Engine::setEngineInfo(const nx::sdk::analytics::IEngineInfo* /*engineInfo*/)
{
    // Do nothing.
}

StringMapResult Engine::setSettings(const IStringMap* /*settings*/)
{
    // There are no DeviceAgent settings for this plugin.
    return nullptr;
}

SettingsResponseResult Engine::pluginSideSettings() const
{
    return nullptr;
}

DeviceAgentResult Engine::obtainDeviceAgent(const IDeviceInfo* deviceInfo)
{
    if (isCompatible(deviceInfo))
        return new DeviceAgent(this, deviceInfo, m_typedManifest);

    return nullptr;
}

StringResult Engine::manifest() const
{
    return new nx::sdk::String(m_manifest);
}

const EventType* Engine::eventTypeById(const QString& id) const noexcept
{
    const auto it = std::find_if(
        m_typedManifest.eventTypes.cbegin(),
        m_typedManifest.eventTypes.cend(),
        [&id](const EventType& eventType) { return eventType.id == id; });

    return (it != m_typedManifest.eventTypes.cend()) ? &(*it) : nullptr;
}

VoidResult Engine::executeAction(IAction* /*action*/)
{
    return {};
}

void Engine::setHandler(IHandler* /*handler*/)
{
    // TODO: Use the handler for error reporting.
}

bool Engine::isCompatible(const IDeviceInfo* deviceInfo) const
{
    const auto vendor = QString(deviceInfo->vendor()).toLower();
    return vendor.startsWith(kVcaVendor);
}

} // namespace vca
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

namespace {

static const std::string kLibName = "vca_analytics_plugin";

static const std::string kPluginManifest = /*suppress newline*/1 + R"json(
{
    "id": "nx.vca",
    "name": "VCA analytics plugin",
    "description": "Supports built-in analytics on VCA cameras",
    "version": "1.0.0",
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
            return new nx::vms_server_plugins::analytics::vca::Engine(
                dynamic_cast<nx::sdk::analytics::Plugin*>(plugin));
        });
}

} // extern "C"
