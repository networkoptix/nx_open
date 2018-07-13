#include "hanwha_metadata_plugin.h"

#include <map>

#include <QtCore/QString>
#include <QtCore/QUrlQuery>
#include <QtCore/QFile>

#include "hanwha_metadata_manager.h"
#include "hanwha_common.h"
#include "hanwha_attributes_parser.h"
#include "hanwha_string_helper.h"

#include <nx/network/http/http_client.h>
#include <plugins/resource/hanwha/hanwha_cgi_parameters.h>
#include <plugins/resource/hanwha/hanwha_request_helper.h>
#include <nx/api/analytics/device_manifest.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/scope_guard.h>

namespace nx {
namespace mediaserver {
namespace plugins {

namespace {

static const char* kPluginName = "Hanwha metadata plugin";
static const QString kSamsungTechwinVendor("samsung");
static const QString kHanwhaTechwinVendor("hanwha");

static const QString kVideoAnalytics("VideoAnalytics");
static const QString kAudioAnalytics("AudioAnalytics");
static const QString kQueueEvent("QueueEvent");
static const std::map<QString, QString> kSpecialEventTypes = {
    {kVideoAnalytics, kVideoAnalytics},
    {kAudioAnalytics, kAudioAnalytics},
    {kQueueEvent, "Queue"}
};

static const std::chrono::milliseconds kAttributesTimeout(4000);
static const QString kAttributesPath("/stw-cgi/attributes.cgi/cgis");

QString specialEventName(const QString& eventName)
{
    const auto itr = kSpecialEventTypes.find(eventName);
    if (itr == kSpecialEventTypes.cend())
        return QString();

    return itr->second;
}

} // namespace

using namespace nx::sdk;
using namespace nx::sdk::metadata;
using namespace nx::mediaserver_core::plugins;

HanwhaMetadataPlugin::SharedResources::SharedResources(
    const QString& sharedId,
    const Hanwha::DriverManifest& driverManifest,
    const QUrl& url,
    const QAuthenticator& auth)
    :
    monitor(std::make_unique<HanwhaMetadataMonitor>(driverManifest, url, auth)),
    sharedContext(std::make_shared<HanwhaSharedResourceContext>(sharedId))
{
    sharedContext->setResourceAccess(url, auth);
}

void HanwhaMetadataPlugin::SharedResources::setResourceAccess(
    const QUrl& url,
    const QAuthenticator& auth)
{
    sharedContext->setResourceAccess(url, auth);
    monitor->setResourceAccess(url, auth);
}

HanwhaMetadataPlugin::HanwhaMetadataPlugin()
{
    QFile f(":/hanwha/manifest.json");
    if (f.open(QFile::ReadOnly))
        m_manifest = f.readAll();
    m_driverManifest = QJson::deserialized<Hanwha::DriverManifest>(m_manifest);
}

void* HanwhaMetadataPlugin::queryInterface(const nxpl::NX_GUID& interfaceId)
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

CameraManager* HanwhaMetadataPlugin::obtainCameraManager(
    const CameraInfo& cameraInfo,
    Error* outError)
{
    *outError = Error::noError;

    const auto vendor = QString(cameraInfo.vendor).toLower();

    if (!vendor.startsWith(kHanwhaTechwinVendor) && !vendor.startsWith(kSamsungTechwinVendor))
        return nullptr;

    auto sharedRes = sharedResources(cameraInfo);
    auto sharedResourceGuard = makeScopeGuard(
        [&sharedRes, &cameraInfo, this]()
        {
            if (sharedRes->managerCounter == 0)
                m_sharedResources.remove(QString::fromUtf8(cameraInfo.sharedId));
        });

    auto supportedEvents = fetchSupportedEvents(cameraInfo);
    if (!supportedEvents)
        return nullptr;

    nx::api::AnalyticsDeviceManifest deviceManifest;
    deviceManifest.supportedEventTypes = *supportedEvents;

    auto manager = new HanwhaMetadataManager(this);
    manager->setCameraInfo(cameraInfo);
    manager->setDeviceManifest(QJson::serialized(deviceManifest));
    manager->setDriverManifest(driverManifest());

    ++sharedRes->managerCounter;

    return manager;
}

const char* HanwhaMetadataPlugin::capabilitiesManifest(Error* error) const
{
    *error = Error::noError;
    return m_manifest.constData();
}

boost::optional<QList<QnUuid>> HanwhaMetadataPlugin::fetchSupportedEvents(
    const CameraInfo& cameraInfo)
{
    using namespace nx::mediaserver_core::plugins;

    auto sharedRes = sharedResources(cameraInfo);

    const auto information = sharedRes->sharedContext->information();
    if (!information || !information->cgiParameters.isValid())
        return boost::none;

    const auto eventStatuses = sharedRes->sharedContext->eventStatuses();
    if (!eventStatuses || !eventStatuses->isSuccessful())
        return boost::none;

    return eventsFromParameters(
        information->cgiParameters, eventStatuses.value, cameraInfo.channel);
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

    QSet<QnUuid> result;

    auto supportedEvents = supportedEventsParameter->possibleValues();
    for (const auto& eventName: supportedEvents)
    {
        bool gotValidParameter = false;

        auto guid = m_driverManifest.eventTypeByName(eventName);
        if (!guid.isNull())
            result.insert(guid);

        const auto altEventName = specialEventName(eventName);
        if (!altEventName.isEmpty())
        {
            const auto parameters = eventStatuses.response();
            for (const auto& entry: parameters)
            {
                const auto& fullEventName = entry.first;
                const bool isMatched = fullEventName.startsWith(
                    lm("Channel.%1.%2.").args(channel, altEventName));

                if (isMatched)
                {
                    guid = m_driverManifest.eventTypeByName(fullEventName);
                    if (!guid.isNull())
                        result.insert(guid);;
                }
            }
        }
    }

    return QList<QnUuid>::fromSet(result);
}

const Hanwha::DriverManifest& HanwhaMetadataPlugin::driverManifest() const
{
    return m_driverManifest;
}

HanwhaMetadataMonitor* HanwhaMetadataPlugin::monitor(
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
    const nx::sdk::CameraInfo& cameraInfo)
{
    const QUrl url(cameraInfo.url);

    QAuthenticator auth;
    auth.setUser(cameraInfo.login);
    auth.setPassword(cameraInfo.password);

    QnMutexLocker lock(&m_mutex);
    auto sharedResourcesItr = m_sharedResources.find(cameraInfo.sharedId);
    if (sharedResourcesItr == m_sharedResources.cend())
    {
        sharedResourcesItr = m_sharedResources.insert(
            cameraInfo.sharedId,
            std::make_shared<SharedResources>(
                cameraInfo.sharedId,
                driverManifest(),
                url,
                auth));
    }
    else
    {
        sharedResourcesItr.value()->setResourceAccess(url, auth);
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
