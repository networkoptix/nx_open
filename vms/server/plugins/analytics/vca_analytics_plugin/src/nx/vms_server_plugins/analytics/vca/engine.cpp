#include "engine.h"

#include <QtCore/QString>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <nx/fusion/model_functions.h>

#include <nx/vms_server_plugins/utils/uuid.h>

#include <nx/sdk/common/string.h>

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

Engine::Engine(nx::sdk::analytics::common::Plugin* plugin): m_plugin(plugin)
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

void Engine::setSettings(const nx::sdk::IStringMap* settings)
{
    // There are no DeviceAgent settings for this plugin.
}

nx::sdk::IStringMap* Engine::pluginSideSettings() const
{
    return nullptr;
}

nx::sdk::analytics::IDeviceAgent* Engine::obtainDeviceAgent(
    const DeviceInfo* deviceInfo, Error* outError)
{
    if (isCompatible(deviceInfo))
        return new DeviceAgent(this, *deviceInfo, m_typedManifest);

    return nullptr;
}

const nx::sdk::IString* Engine::manifest(Error* error) const
{
    *error = Error::noError;
    return new nx::sdk::common::String(m_manifest);
}

const EventType* Engine::eventTypeById(const QString& id) const noexcept
{
    const auto it = std::find_if(
        m_typedManifest.eventTypes.cbegin(),
        m_typedManifest.eventTypes.cend(),
        [&id](const EventType& eventType) { return eventType.id == id; });

    return (it != m_typedManifest.eventTypes.cend()) ? &(*it) : nullptr;
}

void Engine::executeAction(Action* /*action*/, Error* /*outError*/)
{
}

nx::sdk::Error Engine::setHandler(nx::sdk::analytics::IEngine::IHandler* /*handler*/)
{
    // TODO: Use the handler for error reporting.
    return nx::sdk::Error::noError;
}

bool Engine::isCompatible(const nx::sdk::DeviceInfo* deviceInfo) const
{
    const auto vendor = QString(deviceInfo->vendor).toLower();
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
    "engineSettingsModel": ""
}
)json";

} // namespace

extern "C" {

NX_PLUGIN_API nxpl::PluginInterface* createNxAnalyticsPlugin()
{
    return new nx::sdk::analytics::common::Plugin(
        kLibName,
        kPluginManifest,
        [](nx::sdk::analytics::IPlugin* plugin)
        {
            return new nx::vms_server_plugins::analytics::vca::Engine(
                dynamic_cast<nx::sdk::analytics::common::Plugin*>(plugin));
        });
}

} // extern "C"
