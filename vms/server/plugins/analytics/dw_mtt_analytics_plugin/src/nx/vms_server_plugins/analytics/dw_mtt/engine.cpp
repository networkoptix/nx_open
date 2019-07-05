#include "engine.h"

#include <QtCore/QString>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <nx/fusion/model_functions.h>

#include <nx/sdk/helpers/string.h>

#include "device_agent.h"
#include "log.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace dw_mtt {

namespace {

static const QString kDwMttVendor("digitalwatchdog");
// Just for information:
// DW VCA camera's vendor string is "cap",
// DW MTT camera's vendor string is "digitalwatchdog"

QString normalize(const QString& name)
{
    QString result = name.toLower().simplified();
    result.replace(" ", "");
    return result;
}

} // namespace

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(Plugin* plugin): m_plugin(plugin)
{
    static const char* const kResourceName=":/dw_mtt/manifest.json";
    static const char* const kFileName = "plugins/dw_mtt/manifest.json";
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
    for (auto& model: m_typedManifest.supportedCameraModels)
        model = normalize(model);
}

void Engine::setEngineInfo(const nx::sdk::analytics::IEngineInfo* /*engineInfo*/)
{
}

void Engine::setSettings(const IStringMap* /*settings*/, IError* outError)
{
    // There are no DeviceAgent settings for this plugin.
}

IStringMap* Engine::pluginSideSettings(IError* /*outError*/) const
{
    return nullptr;
}

IDeviceAgent* Engine::obtainDeviceAgent(const IDeviceInfo* deviceInfo, IError* /*outError*/)
{
    if (!isCompatible(deviceInfo))
        return nullptr;

    return new DeviceAgent(this, deviceInfo, m_typedManifest);
}

const IString* Engine::manifest(IError* /*outError*/) const
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

void Engine::executeAction(IAction* /*action*/, IError* /*outError*/)
{
}

void Engine::setHandler(IHandler* /*handler*/)
{
    // TODO: Use the handler for error reporting.
}

bool Engine::isCompatible(const IDeviceInfo* deviceInfo) const
{
    const auto vendor = normalize(QString(deviceInfo->vendor()));
    const auto model = normalize(QString(deviceInfo->model()));

    if (!vendor.startsWith(kDwMttVendor))
    {
        NX_PRINT << "Unsupported camera vendor: "
            << nx::kit::utils::toString(deviceInfo->vendor());
        return false;
    }

    if (!m_typedManifest.supportsModel(model))
    {
        NX_PRINT << "Unsupported camera model: " << nx::kit::utils::toString(deviceInfo->model());
        return false;
    }

    return true;
}

} // namespace dw_mtt
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

namespace {

static const std::string kLibName = "dw_mtt_analytics_plugin";

static const std::string kPluginManifest = /*suppress newline*/1 + R"json(
{
    "id": "nx.dw_mtt",
    "name": "DW MTT analytics plugin",
    "description": "Supports built-in analytics on Digital Watchdog cameras (MTT series)",
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
            return new nx::vms_server_plugins::analytics::dw_mtt::Engine(
                dynamic_cast<nx::sdk::analytics::Plugin*>(plugin));
        });
}

} // extern "C"
