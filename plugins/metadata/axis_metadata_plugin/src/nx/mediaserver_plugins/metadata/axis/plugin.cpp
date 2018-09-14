#include "plugin.h"

#include <array>
#include <fstream>
#include <string>

#include <QtCore/QString>
#include <QtCore/QUrlQuery>
#include <QtCore/QFileInfo>
#include <QtCore/QFile>

#include <nx/network/http/http_client.h>
#include <nx/fusion/model_functions.h>
#include <nx/mediaserver_plugins/utils/uuid.h>
#include <nx/kit/debug.h>

#include "manager.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace axis {

namespace {

const char* const kPluginName = "Axis metadata plugin";
const QString kAxisVendor("axis");
const QString kSoapPath("/vapix/services");

} // namespace

using namespace nx::sdk;
using namespace nx::sdk::metadata;

Plugin::Plugin()
{
    QFile f(":/axis/manifest.json");
    if (f.open(QFile::ReadOnly))
        m_manifest = f.readAll();
    {
        QFile file("plugins/axis/manifest.json");
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

void Plugin::setDeclaredSettings(const nxpl::Setting* settings, int count)
{
}

void Plugin::setPluginContainer(nxpl::PluginInterface* pluginContainer)
{
}

void Plugin::setLocale(const char* locale)
{
}

CameraManager* Plugin::obtainCameraManager(
    const CameraInfo* cameraInfo,
    Error* outError)
{
    *outError = Error::noError;

    const auto vendor = QString(cameraInfo->vendor).toLower();
    if (!vendor.startsWith(kAxisVendor))
        return nullptr;

    AnalyticsDriverManifest events = fetchSupportedEvents(*cameraInfo);
    if (events.outputEventTypes.empty())
        return nullptr;

    return new Manager(*cameraInfo, events);
}

const char* Plugin::capabilitiesManifest(Error* error) const
{
    *error = Error::noError;
    return m_manifest.constData();
}

AnalyticsDriverManifest Plugin::fetchSupportedEvents(const CameraInfo& cameraInfo)
{
    AnalyticsDriverManifest result;
    const char* const ip_port = cameraInfo.url + sizeof("http://") - 1;
    nx::axis::CameraController axisCameraController(ip_port, cameraInfo.login,
        cameraInfo.password);
    if (!axisCameraController.readSupportedEvents())
        return result;

    // Only some rules are useful.
    std::vector<std::string> allowedTopics;
    for (const auto& topic: m_typedManifest.allowedTopics)
        allowedTopics.push_back(topic.toStdString());
    axisCameraController.filterSupportedEvents(allowedTopics);

    std::vector<std::string> forbiddenDescriptions;
    for (const auto& description: m_typedManifest.forbiddenDescriptions)
        forbiddenDescriptions.push_back(description.toStdString());
    axisCameraController.removeForbiddenEvents(forbiddenDescriptions);

    const auto& src = axisCameraController.suppotedEvents();
    std::transform(src.begin(), src.end(), std::back_inserter(result.outputEventTypes),
        [](const nx::axis::SupportedEventType& eventType) {return AnalyticsEventType(eventType); });

    // Being uncommented, the next code line allows to get the list of supported events
    // in the same json format that is used in "static-resources/manifest.json".
    // All you need is to stop on a breakpoint and copy the text form your debugger.
    //QString textForManifest = QJson::serialized(result);

    return result;
}

void Plugin::executeAction(Action* /*action*/, Error* /*outError*/)
{
    // Do nothing.
}

} // axis
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

extern "C" {

NX_PLUGIN_API nxpl::PluginInterface* createNxMetadataPlugin()
{
    return new nx::mediaserver_plugins::metadata::axis::Plugin();
}

} // extern "C"
