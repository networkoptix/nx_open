#include "plugin.h"

#include <array>
#include <fstream>
#include <string>
#include <memory>

#include <QtCore/QString>
#include <QtCore/QUrlQuery>
#include <QtCore/QFile>

#include <nx/network/http/http_client.h>
#include <nx/fusion/model_functions.h>
#include <plugins/plugin_internal_tools.h>

#include <nx/network/http/asynchttpclient.h>

#include <nx/kit/debug.h>

#include "manager.h"

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
    QFile f(":/vca/manifest.json");
    if (f.open(QFile::ReadOnly))
        m_manifest = f.readAll();
    else
        NX_PRINT << kPluginName <<" can not open resource \":/vca/manifest.json\".";

    m_typedManifest = QJson::deserialized<Vca::VcaAnalyticsDriverManifest>(m_manifest);
}

void* Plugin::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_MetadataPlugin)
    {
        addRef();
        return static_cast<AbstractMetadataPlugin*>(this);
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

AbstractMetadataManager* Plugin::managerForResource(
    const ResourceInfo& resourceInfo,
    Error* outError)
{
    *outError = Error::noError;
    const auto vendor = QString(resourceInfo.vendor).toLower();
    if (!vendor.startsWith(kVcaVendor))
    {
        NX_PRINT << kPluginName <<" got unsupported resource. Manager can not be created.";
        return nullptr;
    }
    else
    {
        NX_PRINT << kPluginName << " creates new manager.";
        return new Manager(this, resourceInfo, m_typedManifest);
    }
}

AbstractSerializer* Plugin::serializerForType(
    const nxpl::NX_GUID& typeGuid,
    Error* outError)
{
    *outError = Error::typeIsNotSupported;
    return nullptr;
}

const char* Plugin::capabilitiesManifest(Error* error) const
{
    *error = Error::noError;
    return m_manifest.constData();
}

const Vca::VcaAnalyticsEventType& Plugin::eventByInternalName(
    const QString& internalName) const noexcept
{
    // There are only few elements, so linear search is the fastest and the most simple.
    const auto it = std::find_if(
        m_typedManifest.outputEventTypes.cbegin(),
        m_typedManifest.outputEventTypes.cend(),
        [&internalName](const Vca::VcaAnalyticsEventType& event)
    {
        return event.internalName == internalName;
    });

    return
        (it != m_typedManifest.outputEventTypes.cend())
        ? *it
        : m_emptyEvent;
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
