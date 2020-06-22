#include "engine.h"

#include <algorithm>

#include <QtCore/QString>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <nx/fusion/model_functions.h>

#include <nx/sdk/helpers/string.h>

#include "device_agent.h"
#include "log.h"

namespace nx::vms_server_plugins::analytics::dw_tvt {

namespace {

static const QStringList kDwTvtVendors{
    "tvt", // vendor name received with manual discovery
    "ipc", // vendor name received with auto discovery
    "customer", // temporary enabled vendor name for pre-release devices
    "digitalwatchdog"
};

// Just for information:
// DW VCA camera's vendor string is "cap",
// DW TVT camera's vendor string is "tvt" or "ipc" or (temporarily?) "customer"

QString toLowerSpaceless(const QString& name)
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
    static const char* const kResourceName=":/dw_tvt/manifest.json";
    static const char* const kFileName = "plugins/dw_tvt/manifest.json";
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
        model = toLowerSpaceless(model);
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
    if (isCompatible(deviceInfo))
        *outResult = new DeviceAgent(this, deviceInfo, m_typedManifest);
}

void Engine::getManifest(Result<const IString*>* outResult) const
{
    *outResult = new nx::sdk::String(m_manifest);
}

const EventType* Engine::eventTypeById(const QString& id) const noexcept
{
    const auto it = std::find_if(
        m_typedManifest.eventTypes.cbegin(),
        m_typedManifest.eventTypes.cend(),
        [&id](const EventType& eventType) { return eventType.id == id; });

    return (it != m_typedManifest.eventTypes.cend()) ? &(*it) : nullptr;
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
    const auto vendor = toLowerSpaceless(QString(deviceInfo->vendor()));
    const auto model = toLowerSpaceless(QString(deviceInfo->model()));

    if (std::none_of(kDwTvtVendors.begin(), kDwTvtVendors.end(),
        [&vendor](const QString& v) { return vendor.startsWith(v); }))
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

} // nx::vms_server_plugins::analytics::dw_tvt

namespace {

static const std::string kPluginManifest = /*suppress newline*/ 1 + (const char*) R"json(
{
    "id": "nx.dw_tvt",
    "name": "DW TVT analytics plugin",
    "description": "Supports built-in analytics on Digital Watchdog TVT cameras (TD-9xxxE3 series)",
    "version": "1.0.0"
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
            return new nx::vms_server_plugins::analytics::dw_tvt::Engine(
                dynamic_cast<nx::sdk::analytics::Plugin*>(plugin));
        });
}

} // extern "C"
