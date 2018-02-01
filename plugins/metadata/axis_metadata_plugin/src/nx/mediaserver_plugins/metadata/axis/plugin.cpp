﻿#include "plugin.h"

#include <array>
#include <fstream>
#include <string>

#include <QtCore/QString>
#include <QtCore/QUrlQuery>
#include <QtCore/QFile>

#include <nx/network/http/http_client.h>
#include <nx/fusion/model_functions.h>
#include <plugins/plugin_internal_tools.h>

#include "manager.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace axis {

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

Plugin::Plugin()
{
    QFile f(":/axis/manifest.json");
    if (f.open(QFile::ReadOnly))
        m_manifest = f.readAll();

#if 0
    // Definitely this will be useful after pluginManager modification
    nx::api::AnalyticsDriverManifest pluginManifest =
        QJson::deserialized<nx::api::AnalyticsDriverManifest>(m_manifest);
#endif
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
    if (!vendor.startsWith(kAxisVendor))
        return nullptr;

    QList<IdentifiedSupportedEvent> events = fetchSupportedEvents(cameraInfo);
    if (events.empty())
        return nullptr;

    return new Manager(cameraInfo, events);
}

const char* Plugin::capabilitiesManifest(Error* error) const
{
    *error = Error::noError;
    return m_manifest.constData();
}

QList<IdentifiedSupportedEvent> Plugin::fetchSupportedEvents(
    const CameraInfo& cameraInfo)
{
    QList<IdentifiedSupportedEvent> result;
    const char* const ip_port = cameraInfo.url + sizeof("http://") - 1;
    nx::axis::CameraController axisCameraController(ip_port, cameraInfo.login,
        cameraInfo.password);
    if (!axisCameraController.readSupportedEvents())
        return result;

    // Only some rules are useful.
    axisCameraController.filterSupportedEvents({ kRuleEngine, kVideoSource });
    const auto& src = axisCameraController.suppotedEvents();
    std::transform(src.begin(), src.end(), std::back_inserter(result),
        [](const nx::axis::SupportedEvent& event) {return IdentifiedSupportedEvent(event); });

    // Being uncommented, the next code line allows to get the list of supported events
    // in the same json format that is used in "static-resources/manifest.json".
    // All you need is to stop on a breakpoint and copy the text form your debugger.
    //QString textForManifest = serializeEvents(result);

    return result;
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
