#include "hanwha_metadata_plugin.h"
#include "hanwha_metadata_manager.h"
#include "hanwha_common.h"
#include "hanwha_attributes_parser.h"
#include "hanwha_string_helper.h"

#include <QtCore/QString>
#include <QtCore/QUrlQuery>
#include <QtCore/QFile>

#include <nx/network/http/http_client.h>
#include <plugins/resource/hanwha/hanwha_cgi_parameters.h>
#include <plugins/resource/hanwha/hanwha_request_helper.h>
#include <nx/api/analytics/device_manifest.h>
#include <nx/fusion/model_functions.h>

namespace nx {
namespace mediaserver {
namespace plugins {

namespace {

const char* kPluginName = "Hanwha metadata plugin";
const QString kSamsungTechwinVendor = lit("samsung");
const QString kHanwhaTechwinVendor = lit("hanwha");

const QString kVideoAnalytics = lit("VideoAnalytics");
const QString kAudioAnalytics = lit("AudioAnalytics");

const std::chrono::milliseconds kAttributesTimeout(4000);
const QString kAttributesPath = lit("/stw-cgi/attributes.cgi/cgis");

} // namespace

using namespace nx::sdk;
using namespace nx::sdk::metadata;
using namespace nx::mediaserver_core::plugins;

HanwhaMetadataPlugin::SharedResources::SharedResources(
    const QString& sharedId,
    const Hanwha::DriverManifest& driverManifest,
    const nx::utils::Url& url,
    const QAuthenticator& auth)
    :
    monitor(std::make_unique<HanwhaMetadataMonitor>(driverManifest, url, auth)),
    sharedContext(std::make_shared<HanwhaSharedResourceContext>(sharedId))
{
    sharedContext->setRecourceAccess(url, auth);
}

HanwhaMetadataPlugin::HanwhaMetadataPlugin()
{
    QFile f(":manifest.json");
    if (f.open(QFile::ReadOnly))
        m_manifest = f.readAll();
    m_driverManifest = QJson::deserialized<Hanwha::DriverManifest>(m_manifest);
}

void* HanwhaMetadataPlugin::queryInterface(const nxpl::NX_GUID& interfaceId)
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

const char* HanwhaMetadataPlugin::name() const
{
    return kPluginName;
}

void HanwhaMetadataPlugin::setSettings(const nxpl::Setting* settings, int count)
{
    // Do nothing.
}

void HanwhaMetadataPlugin::setPluginContainer(nxpl::PluginInterface* pluginContainer)
{
    // Do nothing.
}

void HanwhaMetadataPlugin::setLocale(const char* locale)
{
    // Do nothing.
}

AbstractMetadataManager* HanwhaMetadataPlugin::managerForResource(
    const ResourceInfo& resourceInfo,
    Error* outError)
{
    *outError = Error::noError;

    const auto vendor = QString(resourceInfo.vendor).toLower();

    if (!vendor.startsWith(kHanwhaTechwinVendor) && !vendor.startsWith(kSamsungTechwinVendor))
        return nullptr;

    auto sharedRes = sharedResources(resourceInfo);
    ++sharedRes->managerCounter;

    auto supportedEvents = fetchSupportedEvents(resourceInfo);
    if (!supportedEvents)
        return nullptr;

    nx::api::AnalyticsDeviceManifest deviceManifest;
    deviceManifest.supportedEventTypes = *supportedEvents;

    auto manager = new HanwhaMetadataManager(this);
    manager->setResourceInfo(resourceInfo);
    manager->setDeviceManifest(QJson::serialized(deviceManifest));
    manager->setDriverManifest(driverManifest());

    return manager;
}

AbstractSerializer* HanwhaMetadataPlugin::serializerForType(
    const nxpl::NX_GUID& typeGuid,
    Error* outError)
{
    *outError = Error::typeIsNotSupported;
    return nullptr;
}

const char* HanwhaMetadataPlugin::capabilitiesManifest(Error* error) const
{
    *error = Error::noError;
    return m_manifest.constData();
}

boost::optional<QList<QnUuid>> HanwhaMetadataPlugin::fetchSupportedEvents(
    const ResourceInfo& resourceInfo)
{
    using namespace nx::mediaserver_core::plugins;

    auto sharedRes = sharedResources(resourceInfo);

    const auto cgiParameters = sharedRes->sharedContext->cgiParamiters();
    if (!cgiParameters.diagnostics || !cgiParameters.value.isValid())
        return boost::none;

    const auto eventStatuses = sharedRes->sharedContext->eventStatuses();
    if (!eventStatuses || !eventStatuses->isSuccessful())
        return boost::none;

    return eventsFromParameters(cgiParameters.value, eventStatuses.value, resourceInfo.channel);
}

boost::optional<QList<QnUuid>> HanwhaMetadataPlugin::eventsFromParameters(
    const nx::mediaserver_core::plugins::HanwhaCgiParameters& parameters,
    const nx::mediaserver_core::plugins::HanwhaResponse& eventStatuses,
    int channel)
{
    if (!parameters.isValid())
        return boost::none;

    auto supportedEventsParameter = parameters.parameter(
        "eventstatus/eventstatus/monitor/Channel.#.EventType");

    if (!supportedEventsParameter.is_initialized())
        return boost::none;

    QList<QnUuid> result;

    auto supportedEvents = supportedEventsParameter->possibleValues();
    for (const auto& eventName: supportedEvents)
    {
        bool gotValidParameter = false;

        auto guid = m_driverManifest.eventTypeByInternalName(eventName);
        if (!guid.isNull())
            result.push_back(guid);

        if (eventName == kVideoAnalytics || eventName == kAudioAnalytics)
        {
            const auto parameters = eventStatuses.response();
            for (const auto& entry : parameters)
            {
                const auto& fullEventName = entry.first;
                const bool isAnalyticsEvent = fullEventName.startsWith(
                    lit("Channel.%1.%2.")
                        .arg(channel)
                        .arg(eventName));

                if (isAnalyticsEvent)
                {
                    guid = m_driverManifest.eventTypeByInternalName(
                        eventName + (".") + fullEventName.split(L'.').last());

                    if (!guid.isNull())
                        result.push_back(guid);
                }
            }
        }
    }

    return result;
}

const Hanwha::DriverManifest& HanwhaMetadataPlugin::driverManifest() const
{
    return m_driverManifest;
}

HanwhaMetadataMonitor* HanwhaMetadataPlugin::monitor(
    const QString& sharedId,
    const nx::utils::Url& url,
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

void HanwhaMetadataPlugin::managerStoppedToUseMonitor(const QString& sharedId)
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

void HanwhaMetadataPlugin::managerIsAboutToBeDestroyed(const QString& sharedId)
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

std::shared_ptr<HanwhaMetadataPlugin::SharedResources> HanwhaMetadataPlugin::sharedResources(
    const nx::sdk::ResourceInfo& resourceInfo)
{
    const nx::utils::Url url(resourceInfo.url);

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
        return new nx::mediaserver::plugins::HanwhaMetadataPlugin();
    }

} // extern "C"
