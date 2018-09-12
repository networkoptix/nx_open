#include "metadata_plugin.h"
#include "metadata_manager.h"
#include "common.h"
#include "string_helper.h"
#include "attributes_parser.h"

#include <QtCore/QString>
#include <QtCore/QUrlQuery>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <nx/network/http/http_client.h>
#include <nx/network/deprecated/asynchttpclient.h>
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
    QFile file(":/hikvision/manifest.json");
    if (file.open(QFile::ReadOnly))
        m_manifest = file.readAll();
    {
        QFile file("plugins/hikvision/manifest.json");
        if (file.open(QFile::ReadOnly))
        {
            NX_INFO(this,
                lm("Switch to external manifest file %1").arg(QFileInfo(file).absoluteFilePath()));
            m_manifest = file.readAll();
        }
    }

    bool success = false;
    m_driverManifest = QJson::deserialized<Hikvision::DriverManifest>(
        m_manifest, Hikvision::DriverManifest(), &success);
    if (!success)
        NX_WARNING(this, lm("Can't deserialize driver manifest file"));
}

void* MetadataPlugin::queryInterface(const nxpl::NX_GUID& interfaceId)
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

const char* MetadataPlugin::name() const
{
    return kPluginName;
}

void MetadataPlugin::setSettings(const nxpl::Setting* /*settings*/, int /*count*/)
{
    // Do nothing.
}

void MetadataPlugin::setDeclaredSettings(const nxpl::Setting* /*settings*/, int /*count*/)
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

CameraManager* MetadataPlugin::obtainCameraManager(
    const CameraInfo* cameraInfo,
    Error* outError)
{
    *outError = Error::noError;

    const auto vendor = QString(cameraInfo->vendor).toLower();

    if (!vendor.startsWith(kHikvisionTechwinVendor))
        return nullptr;

    auto supportedEvents = fetchSupportedEvents(*cameraInfo);
    if (!supportedEvents)
        return nullptr;

    nx::api::AnalyticsDeviceManifest deviceManifest;
    deviceManifest.supportedEventTypes = *supportedEvents;

    auto manager = new MetadataManager(this);
    manager->setCameraInfo(*cameraInfo);
    manager->setDeviceManifest(QJson::serialized(deviceManifest));
    manager->setDriverManifest(driverManifest());

    return manager;
}

const char* MetadataPlugin::capabilitiesManifest(Error* error) const
{
    *error = Error::noError;
    return m_manifest.constData();
}

QList<QString> MetadataPlugin::parseSupportedEvents(const QByteArray& data)
{
    QList<QString> result;
    auto supportedEvents = hikvision::AttributesParser::parseSupportedEventsXml(data);
    if (!supportedEvents)
        return result;
    for (const auto& internalName: *supportedEvents)
    {
        const QString eventTypeId = m_driverManifest.eventTypeByInternalName(internalName);
        if (!eventTypeId.isNull())
        {
            result << eventTypeId;
            const auto descriptor = m_driverManifest.eventTypeDescriptorById(eventTypeId);
            for (const auto& dependedName: descriptor.dependedEvent.split(','))
            {
                auto descriptor = m_driverManifest.eventTypeDescriptorByInternalName(dependedName);
                if (!descriptor.id.isNull())
                    result << descriptor.id;
            }
        }
    }
    return result;
}

boost::optional<QList<QString>> MetadataPlugin::fetchSupportedEvents(
    const CameraInfo& cameraInfo)
{
    auto& data = m_cachedDeviceData[cameraInfo.sharedId];
    if (!data.hasExpired())
        return data.supportedEventTypes;

    nx::utils::Url url(cameraInfo.url);
    url.setPath("/ISAPI/Event/triggersCap");
    url.setUserInfo(cameraInfo.login);
    url.setPassword(cameraInfo.password);
    int statusCode = 0;
    QByteArray buffer;
    if (nx::network::http::downloadFileSync(url, &statusCode, &buffer) != SystemError::noError ||
        statusCode != nx::network::http::StatusCode::ok)
    {
        NX_WARNING(this,lm("Can't fetch supported events for device %1. HTTP status code: %2").
            arg(cameraInfo.url).arg(statusCode));
        return boost::optional<QList<QString>>();
    }
    NX_DEBUG(this, lm("Device url %1. RAW list of supported analytics events: %2").
        arg(cameraInfo.url).arg(buffer));

    data.supportedEventTypes = parseSupportedEvents(buffer);
    return data.supportedEventTypes;
}

const Hikvision::DriverManifest& MetadataPlugin::driverManifest() const
{
    return m_driverManifest;
}

void MetadataPlugin::executeAction(Action* /*action*/, Error* /*outError*/)
{
    // Do nothing.
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
