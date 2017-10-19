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

    auto url = QUrl(QString(resourceInfo.url));

    QAuthenticator auth;
    auth.setUser(resourceInfo.login);
    auth.setPassword(resourceInfo.password);

    auto supportedEvents = fetchSupportedEvents(url, auth, resourceInfo.channel);
    if (!supportedEvents)
        return nullptr;

    nx::api::AnalyticsDeviceManifest deviceManifest;
    deviceManifest.supportedEventTypes = *supportedEvents;

    const QString sharedId(resourceInfo.sharedId);
    HanwhaMetadataManager* manager;

    {
        QnMutexLocker lock(&m_mutex);
        auto monitorItr = m_monitors.find(sharedId);
        if (monitorItr == m_monitors.cend())
        {
            monitorItr = m_monitors.insert(
                sharedId,
                std::make_shared<MonitorCounter>(
                    std::make_unique<HanwhaMetadataMonitor>(driverManifest(), url, auth)));
        }

        auto monitorCounter = monitorItr.value();
        ++monitorCounter->counter;

        manager = new HanwhaMetadataManager(this);
        manager->setResourceInfo(resourceInfo);
        manager->setDeviceManifest(QJson::serialized(deviceManifest));
        manager->setDriverManifest(driverManifest());
        manager->setMonitor(monitorCounter->monitor.get());
    }

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
    const QUrl& url,
    const QAuthenticator& auth,
    int channel)
{
    using namespace nx::mediaserver_core::plugins;

    HanwhaRequestHelper helper(auth, url.toString());
    const auto parameters = helper.fetchCgiParameters(lit("cgis"));

    if (!parameters.isValid())
        return boost::none;

    const auto eventStatuses = helper.check(lit("eventstatus/eventstatus"));

    if (!eventStatuses.isSuccessful())
        return boost::none;

    return eventsFromParameters(parameters, eventStatuses, channel);
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

        if (eventName == kVideoAnalytics)
        {
            auto supportedAreaAnalyticsParameter = parameters.parameter(
                "eventsources/videoanalysis/set/DefinedArea.#.Mode");

            gotValidParameter = supportedAreaAnalyticsParameter.is_initialized()
                && supportedAreaAnalyticsParameter->isValid();

            if (gotValidParameter)
            {
                auto supportedAreaAnalytics =
                    supportedAreaAnalyticsParameter->possibleValues();

                for (const auto& videoAnalytics: supportedAreaAnalytics)
                {
                    guid = m_driverManifest.eventTypeByInternalName(
                        lit("%1.%2")
                            .arg(kVideoAnalytics)
                            .arg(videoAnalytics));

                    if (!guid.isNull())
                        result.push_back(guid);
                }
            }
            else
            {
                const auto parameters = eventStatuses.response();
                for (const auto& entry: parameters)
                {
                    const auto& eventName = entry.first;
                    if (eventName.startsWith(lit("Channel.%1.VideoAnalytics.").arg(channel)))
                    {
                        guid = m_driverManifest.eventTypeByInternalName(
                            lit("VideoAnalytics.") + eventName.split(L'.').last());

                        if (!guid.isNull())
                            result.push_back(guid);
                    }
                }
            }

            auto supportedLineAnalyticsParameter = parameters.parameter(
                "eventsources/videoanalysis/set/Line.#.Mode");

            gotValidParameter = supportedLineAnalyticsParameter.is_initialized()
                && supportedLineAnalyticsParameter->isValid();

            if (gotValidParameter)
            {
                guid = m_driverManifest.eventTypeByInternalName(
                    lit("%1.%2")
                    .arg(kVideoAnalytics)
                    .arg(lit("Passing")));

                if (!guid.isNull())
                    result.push_back(guid);
            }

        }

        if (eventName == kAudioAnalytics)
        {
            auto supportedAudioTypesParameter = parameters.parameter(
                "eventsources/audioanalysis/set/SoundType");

            gotValidParameter = supportedAudioTypesParameter.is_initialized()
                && supportedAudioTypesParameter->isValid();

            if (gotValidParameter)
            {
                auto supportedAudioTypes = supportedAudioTypesParameter->possibleValues();
                for (const auto& audioAnalytics : supportedAudioTypes)
                {
                    guid = m_driverManifest.eventTypeByInternalName(
                        lit("%1.%2")
                            .arg(kAudioAnalytics)
                            .arg(audioAnalytics));

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

void HanwhaMetadataPlugin::managerStoppedToUseMonitor(const QString& sharedId)
{
    QnMutexLocker lock(&m_mutex);
    auto monitorCounter = m_monitors.value(sharedId);
    if (!monitorCounter)
        return;

    --monitorCounter->counter;
    NX_ASSERT(monitorCounter->counter >= 0);
    if (monitorCounter->counter <= 0)
        m_monitors.remove(sharedId);
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
