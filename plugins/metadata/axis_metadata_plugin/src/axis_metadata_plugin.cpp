#include "axis_metadata_plugin.h"

#include <string>
#include <fstream>

#include <QtCore/QString>
#include <QtCore/QUrlQuery>
#include <QtCore/QFile>

#include <nx/network/http/http_client.h>
#include <nx/api/analytics/device_manifest.h>
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

// Two soap topics essential for video analytics.
const char* const kRuleEngine = "tns1:RuleEngine";
const char* const kVideoSource = "tns1:VideoSource";

} // namespace

using namespace nx::sdk;
using namespace nx::sdk::metadata;

AxisMetadataPlugin::SharedResources::SharedResources(
    const QString& sharedId,
    const Axis::DriverManifest& driverManifest,
    const QUrl& url,
    const QAuthenticator& auth)
    :
    monitor(std::make_unique<AxisMetadataMonitor>(driverManifest, url, auth))
{
}

AxisMetadataPlugin::AxisMetadataPlugin()
{
    QFile f(":manifest.json");
    if (f.open(QFile::ReadOnly))
        m_manifest = f.readAll();
    m_driverManifest = QJson::deserialized<Axis::DriverManifest>(m_manifest);
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

    auto sharedRes = sharedResources(resourceInfo);
    ++sharedRes->managerCounter;

    auto supportedEvents = fetchSupportedEvents(resourceInfo);
    if (!supportedEvents)
        return nullptr;

    sharedRes->supportedEvents = supportedEvents.get();

    nx::api::AnalyticsDeviceManifest deviceManifest;
    deviceManifest.supportedEventTypes = *supportedEvents;

    auto manager = new AxisMetadataManager(this);
    manager->setResourceInfo(resourceInfo);
    manager->setDeviceManifest(QJson::serialized(deviceManifest));
    manager->setDriverManifest(driverManifest());
    manager->m_axisEvents= fetchSupportedAxisEvents(resourceInfo);

    return manager;
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

QList<AxisEvent> AxisMetadataPlugin::fetchSupportedAxisEvents(
    const ResourceInfo& resourceInfo)
{
    QList<AxisEvent> result;
    const char* const ip_port = resourceInfo.url + sizeof("http://") - 1;
    nx::axis::CameraController axisCameraController(ip_port, resourceInfo.login,
        resourceInfo.password);
    if (!axisCameraController.readSupportedEvents())
        return result;
    axisCameraController.filterSupportedEvents({ kRuleEngine, kVideoSource });

    const auto& src = axisCameraController.suppotedEvents();
    std::transform(src.begin(), src.end(), std::back_inserter(result), convertEvent);

    // Being uncommented, the next code line allows to get the list of supported events
    // in the same json format that is used in "static-resources/manifest.json".
    // All you need is to stop on a breakpoint and copy the text form your debugger.
    //QString textForManifest = serializeEvents(result);

    return result;
}

boost::optional<QList<QnUuid>> AxisMetadataPlugin::fetchSupportedEvents(
    const ResourceInfo& resourceInfo)
{
    QList<QnUuid> result;
    const char* const ip_port = resourceInfo.url + sizeof("http://") - 1;
    nx::axis::CameraController axisCameraController(ip_port, resourceInfo.login,
        resourceInfo.password);
    if (!axisCameraController.readSupportedEvents())
        return result;
    axisCameraController.filterSupportedEvents({ kRuleEngine, kVideoSource });

    const auto& src = axisCameraController.suppotedEvents();
    std::transform(src.begin(), src.end(), std::back_inserter(result),
        [](const auto& supportedEvent)
        {
            AxisEvent axisEvent = convertEvent(supportedEvent);
            return nxpt::fromPluginGuidToQnUuid(axisEvent.typeId);
        });
    return result;
}

const Axis::DriverManifest& AxisMetadataPlugin::driverManifest() const
{
    return m_driverManifest;
}

AxisMetadataMonitor* AxisMetadataPlugin::monitor(
    const QString& sharedId,
    const QUrl& url,
    const QAuthenticator& auth)
{
    std::shared_ptr<SharedResources> monitorCounter;
    {
        QnMutexLocker lock(&m_mutex);
        auto sharedResourcesItr = m_sharedResources.find(sharedId);
        if (sharedResourcesItr == m_sharedResources.cend())
        {
            sharedResourcesItr = m_sharedResources.insert(
                sharedId,
                std::make_shared<SharedResources>(
                    sharedId,
                    driverManifest(),
                    url,
                    auth));
        }

        monitorCounter = sharedResourcesItr.value();
        ++monitorCounter->monitorUsageCounter;
    }
    return monitorCounter->monitor.get();
}

void AxisMetadataPlugin::managerStoppedToUseMonitor(const QString& sharedId)
{
    QnMutexLocker lock(&m_mutex);
    auto sharedResources = m_sharedResources.value(sharedId);
    if (!sharedResources)
        return;

    if (sharedResources->monitorUsageCounter)
        --sharedResources->monitorUsageCounter;

    if (sharedResources->monitorUsageCounter <= 0)
        m_sharedResources[sharedId]->monitor->stopMonitoring();
}

void AxisMetadataPlugin::managerIsAboutToBeDestroyed(const QString& sharedId)
{
    QnMutexLocker lock(&m_mutex);
    auto sharedResources = m_sharedResources.value(sharedId);
    if (!sharedResources)
        return;

    --sharedResources->managerCounter;
    NX_ASSERT(sharedResources->managerCounter >= 0);
    if (sharedResources->managerCounter <= 0)
        m_sharedResources.remove(sharedId);
}

std::shared_ptr<AxisMetadataPlugin::SharedResources> AxisMetadataPlugin::sharedResources(
    const QString& sharedId)
{
    auto sharedResourcesItr = m_sharedResources.find(sharedId);
    if (sharedResourcesItr == m_sharedResources.cend())
        return std::shared_ptr<SharedResources>();
    else
        return sharedResourcesItr.value();
}

std::shared_ptr<AxisMetadataPlugin::SharedResources> AxisMetadataPlugin::sharedResources(
    const nx::sdk::ResourceInfo& resourceInfo)
{
    const QUrl url(resourceInfo.url);

    QAuthenticator auth;
    auth.setUser(resourceInfo.login);
    auth.setPassword(resourceInfo.password);

    QnMutexLocker lock(&m_mutex);
    auto sharedResourcesItr = m_sharedResources.find(resourceInfo.sharedId);
    if (sharedResourcesItr == m_sharedResources.cend())
    {
        sharedResourcesItr = m_sharedResources.insert(
            resourceInfo.sharedId,
            std::make_shared<SharedResources>(
                resourceInfo.sharedId,
                driverManifest(),
                url,
                auth));
    }

    return sharedResourcesItr.value();
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
