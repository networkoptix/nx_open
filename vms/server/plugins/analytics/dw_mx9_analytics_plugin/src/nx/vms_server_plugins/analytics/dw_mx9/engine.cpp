#include "engine.h"

#include <algorithm>

#include <QtCore/QString>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <nx/fusion/model_functions.h>

#include <nx/sdk/helpers/string.h>

#include "device_agent.h"
#include "log.h"

namespace nx::vms_server_plugins::analytics::dw_mx9 {

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
    static const QString kResourceNamePattern=":/dw_mx9/%1.json";
    static const QString kFileNamePattern = "plugins/dw_mx9/%1.json";
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

} // namespace nx::vms_server_plugins::analytics::dw_mx9

namespace {

static const std::string kDwMx9PluginManifest = /*suppress newline*/ 1 + (const char*) R"json(
{
    "id": "nx.dw_mx9",
    "name": "DW Mx9 Camera Analytics",
    "description": "Enables in-camera analytics support for DW Mx9 IP Cameras",
    "version": "1.0.1",
    "vendor": "DW MTT"
}
)json";

// Used for 340IPE / 341IPE series.
static const std::string kProvisionPluginManifest = /*suppress newline*/ 1 + (const char*)R"json(
{
    "id": "nx.provision_isr",
    "name": "Provision ISR Camera Analytics",
    "description": "Enables in-camera analytics support for Provision ISR IP Cameras",
    "version": "1.0.1",
    "vendor": "Provision ISR"
}
)json";

static const std::string kDwMx9ManifestName = "manifest_dw_mx9";
static const std::string kProvisionManifestName = "manifest_provision";

enum VendorIndex: int
{
    dwMx9VendorIndex = 0,
    provisionVendorIndex,
};

} // namespace

extern "C" {

NX_PLUGIN_API nx::sdk::IPlugin* createNxPluginByIndex(int index)
{
    switch ((VendorIndex) index)
    {
        case dwMx9VendorIndex:
            return new nx::sdk::analytics::Plugin(
                kDwMx9PluginManifest,
                [](nx::sdk::analytics::Plugin* plugin)
                {
                    return new nx::vms_server_plugins::analytics::dw_mx9::Engine(
                        plugin, kDwMx9ManifestName);
                });

        case provisionVendorIndex:
            return new nx::sdk::analytics::Plugin(
                kProvisionPluginManifest,
                [](nx::sdk::analytics::Plugin* plugin)
                {
                    return new nx::vms_server_plugins::analytics::dw_mx9::Engine(
                        plugin, kProvisionManifestName);
                });

        default:
            return nullptr;
    }
}

/* For compatibility with Servers which do not support multi-IPlugin plugins. */
NX_PLUGIN_API nx::sdk::IPlugin* createNxPlugin()
{
    return createNxPluginByIndex(0);
}

} // extern "C"
