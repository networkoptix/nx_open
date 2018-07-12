#include "plugin.h"

#include <QtCore/QString>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <nx/fusion/model_functions.h>

#include <nx/mediaserver_plugins/utils/uuid.h>

#include "manager.h"
#include "log.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace vca {

namespace {

static const char* const kPluginName = "VCA metadata plugin";
static const QString kVcaVendor("cap");

} // namespace

using namespace nx::sdk;
using namespace nx::sdk::metadata;

Plugin::Plugin()
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
    m_typedManifest = QJson::deserialized<AnalyticsDriverManifest>(m_manifest);
}

void* Plugin::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_Plugin)
    {
        addRef();
        return static_cast<Plugin*>(this);
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

const char* Plugin::name() const
{
    return kPluginName;
}

void Plugin::setSettings(const nxpl::Setting* settings, int count)
{
}

void Plugin::setPluginContainer(nxpl::PluginInterface* pluginContainer)
{
}

void Plugin::setLocale(const char* locale)
{
}

CameraManager* Plugin::obtainCameraManager(const CameraInfo& cameraInfo, Error* outError)
{
    *outError = Error::noError;
    const auto vendor = QString(cameraInfo.vendor).toLower();
    if (vendor.startsWith(kVcaVendor))
        return new Manager(this, cameraInfo, m_typedManifest);
    else
        return nullptr;
}

const char* Plugin::capabilitiesManifest(Error* error) const
{
    *error = Error::noError;
    return m_manifest.constData();
}

const AnalyticsEventType* Plugin::eventByUuid(const QnUuid& uuid) const noexcept
{
    const auto it = std::find_if(
        m_typedManifest.outputEventTypes.cbegin(),
        m_typedManifest.outputEventTypes.cend(),
        [&uuid](const AnalyticsEventType& event)
        {
            return event.typeId == uuid;
        });

    return (it != m_typedManifest.outputEventTypes.cend()) ? &(*it) : nullptr;
}

void Plugin::executeAction(Action* /*action*/, Error* /*outError*/)
{
    // Do nothing.
}

} // namespace vca
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

extern "C" {

NX_PLUGIN_API nxpl::PluginInterface* createNxMetadataPlugin()
{
    return new nx::mediaserver_plugins::metadata::vca::Plugin();
}

} // extern "C"
