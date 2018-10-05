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
namespace vca {

namespace {

static const char* const kPluginName = "VCA metadata plugin";
static const QString kVcaVendor("cap");

} // namespace

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine()
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

    if (interfaceId == nxpl::IID_Plugin3)
    {
        addRef();
        return static_cast<nxpl::Plugin3*>(this);
    }

    if (interfaceId == nxpl::IID_Plugin2)
    {
        addRef();
        return static_cast<nxpl::Plugin2*>(this);
    }

    if (interfaceId == nxpl::IID_Plugin)
    {
        addRef();
        return static_cast<nxpl::Plugin*>(this);
    }

    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

const char* Engine::name() const
{
    return kPluginName;
}

void Engine::setSettings(const nxpl::Setting* settings, int count)
{
}

void Engine::setPluginContainer(nxpl::PluginInterface* pluginContainer)
{
}

void Engine::setLocale(const char* locale)
{
}

nx::sdk::analytics::DeviceAgent* Engine::obtainDeviceAgent(
    const DeviceInfo* deviceInfo, Error* outError)
{
    const auto vendor = QString(deviceInfo->vendor).toLower();
    if (vendor.startsWith(kVcaVendor))
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

void Engine::executeAction(Action* /*action*/, Error* /*outError*/)
{
    // Do nothing.
}

} // namespace vca
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx

extern "C" {

NX_PLUGIN_API nxpl::PluginInterface* createNxAnalyticsEngine()
{
    return new nx::mediaserver_plugins::analytics::vca::Engine();
}

} // extern "C"
