#include "plugin.h"

#include <map>

#include <QtCore/QString>
#include <QtCore/QUrlQuery>
#include <QtCore/QFile>


#include <nx/network/http/http_client.h>
#include <plugins/resource/hanwha/hanwha_cgi_parameters.h>
#include <nx/api/analytics/device_manifest.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/scope_guard.h>

#include "manager.h"
#include "common.h"
#include "attributes_parser.h"
#include "string_helper.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace hanwha {

namespace {

static const char* const kPluginName = "Hanwha metadata plugin";
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

// TODO: Decide on these unused constants.
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

Plugin::SharedResources::SharedResources(
    const QString& sharedId,
    const Hanwha::DriverManifest& driverManifest,
    const nx::utils::Url& url,
    const QAuthenticator& auth)
    :
    monitor(std::make_unique<MetadataMonitor>(driverManifest, url, auth)),
    sharedContext(std::make_shared<mediaserver_core::plugins::HanwhaSharedResourceContext>(
        sharedId))
{
    sharedContext->setResourceAccess(url, auth);
}

void Plugin::SharedResources::setResourceAccess(
    const nx::utils::Url& url,
    const QAuthenticator& auth)
{
    sharedContext->setResourceAccess(url, auth);
    monitor->setResourceAccess(url, auth);
}

Plugin::Plugin()
{
    QFile f(":/hanwha/manifest.json");
    if (f.open(QFile::ReadOnly))
        m_manifest = f.readAll();
    m_driverManifest = QJson::deserialized<Hanwha::DriverManifest>(m_manifest);
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

void Plugin::setSettings(const nxpl::Setting* /*settings*/, int /*count*/)
{
    // Do nothing.
}

void Plugin::setPluginContainer(nxpl::PluginInterface* /*pluginContainer*/)
{
    // Do nothing.
}

void Plugin::setLocale(const char* /*locale*/)
{
    // Do nothing.
}

CameraManager* Plugin::obtainCameraManager(
    const CameraInfo* cameraInfo,
    Error* outError)
{
    *outError = Error::noError;

    const QString vendor = QString(cameraInfo->vendor).toLower();

    if (!vendor.startsWith(kHanwhaTechwinVendor) && !vendor.startsWith(kSamsungTechwinVendor))
        return nullptr;

    auto sharedRes = sharedResources(*cameraInfo);
    auto sharedResourceGuard = makeScopeGuard(
        [&sharedRes, cameraInfo, this]()
        {
            if (sharedRes->managerCounter == 0)
                m_sharedResources.remove(QString::fromUtf8(cameraInfo->sharedId));
        });

    auto supportedEvents = fetchSupportedEvents(*cameraInfo);
    if (!supportedEvents)
        return nullptr;

    nx::api::AnalyticsDeviceManifest deviceManifest;
    deviceManifest.supportedEventTypes = *supportedEvents;

    auto manager = new Manager(this);
    manager->setCameraInfo(*cameraInfo);
    manager->setDeviceManifest(QJson::serialized(deviceManifest));
    manager->setDriverManifest(driverManifest());

    ++sharedRes->managerCounter;

    return manager;
}

const char* Plugin::capabilitiesManifest(Error* error) const
{
    *error = Error::noError;
    return m_manifest.constData();
}

void Plugin::setDeclaredSettings(const nxpl::Setting* settings, int count)
{
    // Do nothing.
}

void Plugin::executeAction(Action* action, Error* outError)
{
    // Do nothing.
}

boost::optional<QList<QnUuid>> Plugin::fetchSupportedEvents(
    const CameraInfo& cameraInfo)
{
    using namespace nx::mediaserver_core::plugins;

    auto sharedRes = sharedResources(cameraInfo);

    const auto& information = sharedRes->sharedContext->information();
    if (!information)
        return boost::none;
    const auto& cgiParameters = information->cgiParameters;
    if (!cgiParameters.isValid())
        return boost::none;

    const auto& eventStatuses = sharedRes->sharedContext->eventStatuses();
    if (!eventStatuses || !eventStatuses->isSuccessful())
        return boost::none;

    return eventsFromParameters(cgiParameters, eventStatuses.value, cameraInfo.channel);
}

boost::optional<QList<QnUuid>> Plugin::eventsFromParameters(
    const nx::mediaserver_core::plugins::HanwhaCgiParameters& parameters,
    const nx::mediaserver_core::plugins::HanwhaResponse& eventStatuses,
    int channel) const
{
    if (!parameters.isValid())
        return boost::none;

    auto supportedEventsParameter = parameters.parameter(
        "eventstatus/eventstatus/monitor/Channel.#.EventType");

    if (!supportedEventsParameter.is_initialized())
        return boost::none;

    QSet<QnUuid> result;

    const auto& supportedEvents = supportedEventsParameter->possibleValues();
    for (const auto& eventName: supportedEvents)
    {
        auto guid = m_driverManifest.eventTypeByName(eventName);
        if (!guid.isNull())
            result.insert(guid);

        const auto altEventName = specialEventName(eventName);
        if (!altEventName.isEmpty())
        {
            const auto& responseParameters = eventStatuses.response();
            for (const auto& entry: responseParameters)
            {
                const auto& fullEventName = entry.first;
                const bool isMatched = fullEventName.startsWith(
                    lm("Channel.%1.%2.").args(channel, altEventName));

                if (isMatched)
                {
                    guid = m_driverManifest.eventTypeByName(fullEventName);
                    if (!guid.isNull())
                        result.insert(guid);
                }
            }
        }
    }

    return QList<QnUuid>::fromSet(result);
}

const Hanwha::DriverManifest& Plugin::driverManifest() const
{
    return m_driverManifest;
}

MetadataMonitor* Plugin::monitor(
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

void Plugin::managerStoppedToUseMonitor(const QString& sharedId)
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

void Plugin::managerIsAboutToBeDestroyed(const QString& sharedId)
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

std::shared_ptr<Plugin::SharedResources> Plugin::sharedResources(
    const nx::sdk::CameraInfo& cameraInfo)
{
    const nx::utils::Url url(cameraInfo.url);

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

} // namespace hanwha
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

extern "C" {

NX_PLUGIN_API nxpl::PluginInterface* createNxMetadataPlugin()
{
    return new nx::mediaserver_plugins::metadata::hanwha::Plugin();
}

} // extern "C"

