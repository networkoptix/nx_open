#include "engine.h"

#include <QtCore/QString>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <nx/fusion/model_functions.h>

#include <nx/mediaserver_plugins/utils/uuid.h>

#include "device_agent.h"
#include "log.h"

namespace nx {
namespace mediaserver_plugins {
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

Engine::Engine(CommonPlugin* plugin): m_plugin(plugin)
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

void Engine::setSettings(const nxpl::Setting* /*settings*/, int /*count*/)
{
}

nx::sdk::analytics::DeviceAgent* Engine::obtainDeviceAgent(
    const DeviceInfo* deviceInfo, Error* outError)
{
    *outError = Error::noError;
    auto vendor = normalize(QString(deviceInfo->vendor));
    auto model = normalize(QString(deviceInfo->model));

    if (vendor.startsWith(kDwMttVendor) && m_typedManifest.supportsModel(model))
        return new DeviceAgent(this, *deviceInfo, m_typedManifest);
    else
        return nullptr;
}

const char* Engine::manifest(Error* error) const
{
    *error = Error::noError;
    return m_manifest.constData();
}

const EventType* Engine::eventTypeById(const QString& id) const noexcept
{
    const auto it = std::find_if(
        m_typedManifest.outputEventTypes.cbegin(),
        m_typedManifest.outputEventTypes.cend(),
        [&id](const EventType& eventType) { return eventType.id == id; });

    return (it != m_typedManifest.outputEventTypes.cend()) ? &(*it) : nullptr;
}

void Engine::executeAction(
    nx::sdk::analytics::Action* action, nx::sdk::Error* outError)
{
}

} // namespace dw_mtt
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx

extern "C" {

NX_PLUGIN_API nxpl::PluginInterface* createNxAnalyticsPlugin()
{
    return new nx::sdk::analytics::CommonPlugin("dw_mtt_analytics_plugin",
        [](nx::sdk::analytics::Plugin* plugin)
        {
            return new nx::mediaserver_plugins::analytics::dw_mtt::Engine(
                dynamic_cast<nx::sdk::analytics::CommonPlugin*>(plugin));
        });
}

} // extern "C"
