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

QString toLowerSpaceless(const QString& name)
{
    QString result = name.toLower().simplified();
    result.replace(" ", "");
    return result;
}

} // namespace

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(Plugin* plugin, const std::string& manifestName): m_plugin(plugin)
{
    static const QString kResourceNamePattern=":/dw_tvt/%1.json";
    static const QString kFileNamePattern = "plugins/dw_tvt/%1.json";
    QFile f(kResourceNamePattern.arg(QString::fromStdString(manifestName)));
    if (f.open(QFile::ReadOnly))
        m_manifest = f.readAll();
    {
        QFile file(kFileNamePattern.arg(QString::fromStdString(manifestName)));
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
    Result<const ISettingsResponse*>* /*outResult*/, const IStringMap* /*settings*/)
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

    if (std::none_of(m_typedManifest.supportedCameraVendors.begin(),
        m_typedManifest.supportedCameraVendors.end(),
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

static const std::string kTvtPluginManifest = /*suppress newline*/ 1 + (const char*) R"json(
{
    "id": "nx.dw_tvt",
    "name": "DW TVT analytics plugin",
    "description": "Supports built-in analytics on Digital Watchdog TVT cameras (TD-9xxxE3 series and DWC-Mx94Wixxx series)",
    "version": "1.0.1"
}
)json";

static const std::string kProvisionPluginManifest = /*suppress newline*/ 1 + (const char*)R"json(
{
    "id": "nx.provision_isr",
    "name": "Provision ISR analytics plugin",
    "description": "Supports built-in analytics on Provision ISR cameras (340IPE / 341IPE series)",
    "version": "1.0.1"
}
)json";

static const std::string kTvtManifestName = "manifest_tvt";
static const std::string kProvisionManifestName = "manifest_provision";

enum VendorIndex: int {tvtVendor = 0, provisionVendor = 1};

} // namespace

extern "C" {

NX_PLUGIN_API nx::sdk::IPlugin* createNxPluginByIndex(int index)
{
    switch (index)
    {
        case tvtVendor:
            return new nx::sdk::analytics::Plugin(
                kTvtPluginManifest,
                [](nx::sdk::analytics::Plugin* plugin)
                {
                    return new nx::vms_server_plugins::analytics::dw_tvt::Engine(
                        plugin, kTvtManifestName);
                });
        case provisionVendor:
            return new nx::sdk::analytics::Plugin(
                kProvisionPluginManifest,
                [](nx::sdk::analytics::Plugin* plugin)
                {
                    return new nx::vms_server_plugins::analytics::dw_tvt::Engine(
                        plugin, kProvisionManifestName);
                });
        default:
            return nullptr;
    }
}

NX_PLUGIN_API nx::sdk::IPlugin* createNxPlugin()
{
    return createNxPluginByIndex(0);
}

} // extern "C"
