#include "axis_metadata_plugin.h"

#include <string>
#include <fstream>

#include <QtCore/QString>
#include <QtCore/QUrlQuery>
#include <QtCore/QFile>

#include <nx/network/http/http_client.h>
#include <nx/fusion/model_functions.h>
#include <plugins/plugin_internal_tools.h>

#include "axis_metadata_manager.h"
#include "axis_common.h"

namespace nx {
namespace mediaserver {
namespace plugins {

namespace {

const char* const kPluginName = "Axis metadata plugin";
const QString kAxisVendor("axis");
const QString kSoapPath("/vapix/services");

// Two SOAP topics essential for video analytics.
const char* const kRuleEngine = "tns1:RuleEngine";
const char* const kVideoSource = "tns1:VideoSource";

} // namespace

using namespace nx::sdk;
using namespace nx::sdk::metadata;

AxisMetadataPlugin::AxisMetadataPlugin()
{
    QFile f(":manifest.json");
    if (f.open(QFile::ReadOnly))
        m_manifest = f.readAll();
#if 0
    // In current version we get camera capabilies online from camera - not from previously
    // prepared manifest file
    m_driverManifest = QJson::deserialized<Axis::DriverManifest>(m_manifest);
#endif
}

void* AxisMetadataPlugin::queryInterface(const nxpl::NX_GUID& interfaceId)
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

const char* AxisMetadataPlugin::name() const
{
    return kPluginName;
}

void AxisMetadataPlugin::setSettings(const nxpl::Setting* settings, int count)
{
}

void AxisMetadataPlugin::setPluginContainer(nxpl::PluginInterface* pluginContainer)
{
}

void AxisMetadataPlugin::setLocale(const char* locale)
{
}

AbstractMetadataManager* AxisMetadataPlugin::managerForResource(
    const ResourceInfo& resourceInfo,
    Error* outError)
{
    *outError = Error::noError;

    const auto vendor = QString(resourceInfo.vendor).toLower();
    if (!vendor.startsWith(kAxisVendor))
        return nullptr;

    QList<SupportedEventEx> events = fetchSupportedEvents(resourceInfo);
    if (events.empty())
        return nullptr;

    return new AxisMetadataManager(resourceInfo, events);
}

AbstractSerializer* AxisMetadataPlugin::serializerForType(
    const nxpl::NX_GUID& typeGuid,
    Error* outError)
{
    *outError = Error::typeIsNotSupported;
    return nullptr;
}

const char* AxisMetadataPlugin::capabilitiesManifest(Error* error) const
{
    *error = Error::noError;
    return m_manifest.constData();
}

QList<SupportedEventEx> AxisMetadataPlugin::fetchSupportedEvents(
    const ResourceInfo& resourceInfo)
{
    QList<SupportedEventEx> result;
    const char* const ip_port = resourceInfo.url + sizeof("http://") - 1;
    nx::axis::CameraController axisCameraController(ip_port, resourceInfo.login,
        resourceInfo.password);
    if (!axisCameraController.readSupportedEvents())
        return result;
    axisCameraController.filterSupportedEvents({ kRuleEngine, kVideoSource });

    const auto& src = axisCameraController.suppotedEvents();
    std::transform(src.begin(), src.end(), std::back_inserter(result),
        [](const axis::SupportedEvent& event) {return SupportedEventEx(event); });

    // Being uncommented, the next code line allows to get the list of supported events
    // in the same json format that is used in "static-resources/manifest.json".
    // All you need is to stop on a breakpoint and copy the text form your debugger.
    //QString textForManifest = serializeEvents(result);

    return result;
}

} // namespace plugins
} // namespace mediaserver
} // namespace nx

extern "C" {

NX_PLUGIN_API nxpl::PluginInterface* createNxMetadataPlugin()
{
    return new nx::mediaserver::plugins::AxisMetadataPlugin();
}

} // extern "C"
