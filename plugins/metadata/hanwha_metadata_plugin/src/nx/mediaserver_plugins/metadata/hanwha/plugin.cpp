#if defined(ENABLE_HANWHA)

#include "plugin.h"

#include <QtCore/QString>
#include <QtCore/QFile>

#include <nx/network/http/http_client.h>
#include <plugins/resource/hanwha/hanwha_cgi_parameters.h>
#include <nx/api/analytics/device_manifest.h>
#include <nx/fusion/model_functions.h>

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
static const QString kSamsungTechwinVendor = lit("samsung");
static const QString kHanwhaTechwinVendor = lit("hanwha");

static const QString kVideoAnalytics = lit("VideoAnalytics");
static const QString kAudioAnalytics = lit("AudioAnalytics");

// TODO: Decide on these unused constants.
static const std::chrono::milliseconds kAttributesTimeout{4000};
static const QString kAttributesPath = lit("/stw-cgi/attributes.cgi/cgis");

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
    sharedContext->setRecourceAccess(url, auth);
}

Plugin::Plugin()
{
    QFile f(":manifest.json");
    if (f.open(QFile::ReadOnly))
        m_manifest = f.readAll();
    m_driverManifest = QJson::deserialized<Hanwha::DriverManifest>(m_manifest);
}

void* Plugin::queryInterface(const nxpl::NX_GUID& interfaceId)
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

AbstractMetadataManager* Plugin::managerForResource(
    const ResourceInfo& resourceInfo,
    Error* outError)
{
    *outError = Error::noError;

    const QString vendor = QString(resourceInfo.vendor).toLower();

    if (!vendor.startsWith(kHanwhaTechwinVendor) && !vendor.startsWith(kSamsungTechwinVendor))
        return nullptr;

    auto sharedRes = sharedResources(resourceInfo);
    ++sharedRes->managerCounter;

    auto supportedEvents = fetchSupportedEvents(resourceInfo);
    if (!supportedEvents)
        return nullptr;

    nx::api::AnalyticsDeviceManifest deviceManifest;
    deviceManifest.supportedEventTypes = *supportedEvents;

    auto manager = new Manager(this);
    manager->setResourceInfo(resourceInfo);
    manager->setDeviceManifest(QJson::serialized(deviceManifest));
    manager->setDriverManifest(driverManifest());

    return manager;
}

AbstractSerializer* Plugin::serializerForType(
    const nxpl::NX_GUID& /*typeGuid*/,
    Error* outError)
{
    *outError = Error::typeIsNotSupported;
    return nullptr;
}

const char* Plugin::capabilitiesManifest(Error* error) const
{
    *error = Error::noError;
    return m_manifest.constData();
}

boost::optional<QList<QnUuid>> Plugin::fetchSupportedEvents(
    const ResourceInfo& resourceInfo)
{
    using namespace nx::mediaserver_core::plugins;

    auto sharedRes = sharedResources(resourceInfo);

    const auto& cgiParameters = sharedRes->sharedContext->cgiParamiters();
    if (!cgiParameters.diagnostics || !cgiParameters.value.isValid())
        return boost::none;

    const auto& eventStatuses = sharedRes->sharedContext->eventStatuses();
    if (!eventStatuses || !eventStatuses->isSuccessful())
        return boost::none;

    return eventsFromParameters(cgiParameters.value, eventStatuses.value, resourceInfo.channel);
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

    QList<QnUuid> result;

    const auto& supportedEvents = supportedEventsParameter->possibleValues();
    for (const auto& eventName: supportedEvents)
    {
        auto guid = m_driverManifest.eventTypeByInternalName(eventName);
        if (!guid.isNull())
            result.push_back(guid);

        if (eventName == kVideoAnalytics || eventName == kAudioAnalytics)
        {
            const auto& responseParameters = eventStatuses.response();
            for (const auto& entry: responseParameters)
            {
                const auto& fullEventName = entry.first;
                const bool isAnalyticsEvent = fullEventName.startsWith(
                    lit("Channel.%1.%2.")
                        .arg(channel)
                        .arg(eventName));

                if (isAnalyticsEvent)
                {
                    guid = m_driverManifest.eventTypeByInternalName(
                        eventName + "." + fullEventName.split(L'.').last());

                    if (!guid.isNull())
                        result.push_back(guid);
                }
            }
        }
    }

    return result;
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

#endif // defined(ENABLE_HANWHA)

