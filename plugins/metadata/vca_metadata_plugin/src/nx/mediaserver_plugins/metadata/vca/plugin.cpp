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

#include <nx/network/deprecated/asynchttpclient.h>

#include <nx/utils/log/log.h>
#define NX_PRINT NX_UTILS_LOG_STREAM_NO_SPACE( \
    nx::utils::log::Level::debug, lm("vca_metadata_plugin")) NX_PRINT_PREFIX
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
    static const char* const kResourceName=":/vca/manifest.json";
    QFile f(kResourceName);
    if (f.open(QFile::ReadOnly))
        m_manifest = f.readAll();
    else
        NX_PRINT << kPluginName << " can not open resource \"" << kResourceName << "\".";
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

CameraManager* Plugin::obtainCameraManager(
    const CameraInfo& cameraInfo,
    Error* outError)
{
    *outError = Error::noError;
    const auto vendor = QString(cameraInfo.vendor).toLower();
    if (!vendor.startsWith(kVcaVendor))
    {
        NX_PRINT << kPluginName << " got unsupported resource. Manager can not be created.";
        return nullptr;
    }
    else
    {
        NX_PRINT << kPluginName << " creates new manager.";
        return new Manager(this, cameraInfo, m_typedManifest);
    }
}

const char* Plugin::capabilitiesManifest(Error* error) const
{
    *error = Error::noError;
    return m_manifest.constData();
}

const AnalyticsEventType& Plugin::eventByInternalName(
    const QString& internalName) const noexcept
{
    // There are only few elements, so linear search is the fastest and the most simple.
    const auto it = std::find_if(
        m_typedManifest.outputEventTypes.cbegin(),
        m_typedManifest.outputEventTypes.cend(),
        [&internalName](const AnalyticsEventType& event)
        {
            return event.internalName == internalName;
        });

    return
        (it != m_typedManifest.outputEventTypes.cend())
            ? *it
            : m_emptyEvent;
}

const AnalyticsEventType& Plugin::eventByUuid(const QnUuid& uuid) const noexcept
{
    const auto it = std::find_if(
        m_typedManifest.outputEventTypes.cbegin(),
        m_typedManifest.outputEventTypes.cend(),
        [&uuid](const AnalyticsEventType& event)
        {
            return event.eventTypeId == uuid;
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
