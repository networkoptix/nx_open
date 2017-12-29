#include "metadata_plugin.h"
#include "metadata_manager.h"
#include "common.h"
#include "string_helper.h"
#include "attributes_parser.h"

#include <QtCore/QString>
#include <QtCore/QUrlQuery>
#include <QtCore/QFile>

#include <nx/network/http/http_client.h>
#include <nx/api/analytics/device_manifest.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log_main.h>

namespace nx {
namespace mediaserver {
namespace plugins {
namespace hikvision {

namespace {

const char* kPluginName = "Hikvision metadata plugin";
const QString kHikvisionTechwinVendor = lit("hikvision");
static const std::chrono::seconds kCacheTimeout{60};

} // namespace


bool MetadataPlugin::DeviceData::hasExpired() const
{
    return !timeout.isValid() || timeout.hasExpired(kCacheTimeout);
}

using namespace nx::sdk;
using namespace nx::sdk::metadata;

MetadataPlugin::MetadataPlugin()
{
    QFile file(":manifest.json");
    if (file.open(QFile::ReadOnly))
        m_manifest = file.readAll();
    m_driverManifest = QJson::deserialized<Hikvision::DriverManifest>(m_manifest);
}

void* MetadataPlugin::queryInterface(const nxpl::NX_GUID& interfaceId)
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

const char* MetadataPlugin::name() const
{
    return kPluginName;
}

void MetadataPlugin::setSettings(const nxpl::Setting* /*settings*/, int /*count*/)
{
    // Do nothing.
}

void MetadataPlugin::setPluginContainer(nxpl::PluginInterface* pluginContainer)
{
    // Do nothing.
}

void MetadataPlugin::setLocale(const char* locale)
{
    // Do nothing.
}

AbstractMetadataManager* MetadataPlugin::managerForResource(
    const ResourceInfo& resourceInfo,
    Error* outError)
{
    *outError = Error::noError;

    const auto vendor = QString(resourceInfo.vendor).toLower();

    if (!vendor.startsWith(kHikvisionTechwinVendor))
        return nullptr;

    auto supportedEvents = fetchSupportedEvents(resourceInfo);
    if (!supportedEvents)
        return nullptr;

    nx::api::AnalyticsDeviceManifest deviceManifest;
    deviceManifest.supportedEventTypes = *supportedEvents;

    auto manager = new MetadataManager(this);
    manager->setResourceInfo(resourceInfo);
    manager->setDeviceManifest(QJson::serialized(deviceManifest));
    manager->setDriverManifest(driverManifest());

    return manager;
}

AbstractSerializer* MetadataPlugin::serializerForType(
    const nxpl::NX_GUID& typeGuid,
    Error* outError)
{
    *outError = Error::typeIsNotSupported;
    return nullptr;
}

const char* MetadataPlugin::capabilitiesManifest(Error* error) const
{
    *error = Error::noError;
    return m_manifest.constData();
}

QList<QnUuid> MetadataPlugin::parseSupportedEvents(const QByteArray& data)
{
    QList<QnUuid> result;
    auto supportedEvents = hikvision::AttributesParser::parseSupportedEventsXml(data);
    if (!supportedEvents)
        return result;
    for (const auto& internalName: *supportedEvents)
    {
        const QnUuid eventTypeId = m_driverManifest.eventTypeByInternalName(internalName);
        if (!eventTypeId.isNull())
            result << eventTypeId;
    }
    return result;
}

boost::optional<QList<QnUuid>> MetadataPlugin::fetchSupportedEvents(
    const ResourceInfo& resourceInfo)
{
    auto& data = m_cachedDeviceData[resourceInfo.sharedId];
    if (!data.hasExpired())
        return data.supportedEventTypes;

    QUrl url(resourceInfo.url);
    url.setPath("/ISAPI/Event/triggersCap");
    url.setUserInfo(resourceInfo.login);
    url.setPassword(resourceInfo.password);
    int statusCode = 0;
    QByteArray buffer;
    if (nx_http::downloadFileSync(url, &statusCode, &buffer) != SystemError::noError ||
        statusCode != nx_http::StatusCode::ok)
    {
        NX_WARNING(this,lm("Can't fetch supported events for device %1. HTTP status code: %2").
            arg(resourceInfo.url).arg(statusCode));
        return boost::optional<QList<QnUuid>>();
    }
    NX_DEBUG(this, lm("Device url %1. RAW list of supported analytics events: %2").
        arg(resourceInfo.url).arg(buffer));

    data.supportedEventTypes = parseSupportedEvents(buffer);

    return data.supportedEventTypes;
}

const Hikvision::DriverManifest& MetadataPlugin::driverManifest() const
{
    return m_driverManifest;
}

} // namespace hikvision
} // namespace plugins
} // namespace mediaserver
} // namespace nx

extern "C" {

    NX_PLUGIN_API nxpl::PluginInterface* createNxMetadataPlugin()
    {
        return new nx::mediaserver::plugins::hikvision::MetadataPlugin();
    }

} // extern "C"
