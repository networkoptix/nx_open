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

namespace nx {
namespace mediaserver {
namespace plugins {

namespace {

const char* kPluginName = "Hanwha metadata plugin";
const QString kSamsungTechwinVendor = lit("samsung techwin");
const QString kHanwhaTechwinVendor = lit("hanwha techwin");

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
    m_parsedManifest = QJson::deserialized<Hanwha::DriverManifest>(m_manifest);
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

    auto vendor = QString(resourceInfo.vendor).toLower();
    if (vendor != kHanwhaTechwinVendor && vendor != kSamsungTechwinVendor)
        return nullptr;

    auto url = QUrl(QString(resourceInfo.url));

    QAuthenticator auth;
    auth.setUser(resourceInfo.login);
    auth.setPassword(resourceInfo.password);

    auto supportedEvents = fetchSupportedEvents(url, auth);
    if (!supportedEvents)
        return nullptr;

    auto manifest = buildDeviceManifest(*supportedEvents);
    auto manager = new HanwhaMetadataManager();

    manager->setResourceInfo(resourceInfo);
    manager->setCapabilitiesManifest(manifest);

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

boost::optional<std::vector<QnUuid>> HanwhaMetadataPlugin::fetchSupportedEvents(
    const QUrl& url,
    const QAuthenticator& auth)
{
    using namespace nx::mediaserver_core::plugins;

    nx_http::HttpClient httpClient;
    httpClient.setUserName(auth.user());
    httpClient.setUserPassword(auth.password());
    httpClient.setSendTimeoutMs(kAttributesTimeout.count());
    httpClient.setMessageBodyReadTimeoutMs(kAttributesTimeout.count());
    httpClient.setResponseReadTimeoutMs(kAttributesTimeout.count());

    if (!httpClient.doGet(buildAttributesUrl(url)))
        return boost::none;

    nx::Buffer buffer;
    while (!httpClient.eof())
        buffer.append(httpClient.fetchMessageBodyBuffer());

    auto statusCode = (nx_http::StatusCode::Value)httpClient.response()->statusLine.statusCode;
    HanwhaCgiParameters parameters(buffer, statusCode);

    return eventsFromParameters(parameters);
}

QUrl HanwhaMetadataPlugin::buildAttributesUrl(const QUrl& resourceUrl) const
{
    auto url = resourceUrl;
    url.setPath(kAttributesPath);
    url.setQuery(QUrlQuery());

    return url;
}

QByteArray HanwhaMetadataPlugin::buildDeviceManifest(
    const std::vector<QnUuid>& supportedEvents) const
{
    QString manifest = kDeviceManifestTemplate;
    QString supportedEventsStr;

    for (auto i = 0; i < supportedEvents.size(); ++i)
    {
        supportedEventsStr.append(lit("\"%1\"").arg(supportedEvents[i].toString()));
        if (i < supportedEvents.size() - 1)
            supportedEventsStr.append(",\r\n");
    }

    return manifest.arg(supportedEventsStr).toUtf8();
}

boost::optional<std::vector<QnUuid>> HanwhaMetadataPlugin::eventsFromParameters(
    const nx::mediaserver_core::plugins::HanwhaCgiParameters& parameters)
{
    if (!parameters.isValid())
        return boost::none;

    auto supportedEventsParameter = parameters.parameter(
        "eventstatus/eventstatus/monitor/Channel.#.EventType");

    if (!supportedEventsParameter.is_initialized())
        return boost::none;

    std::vector<QnUuid> result;

    auto supportedEvents = supportedEventsParameter->possibleValues();
    for (const auto& eventName : supportedEvents)
    {
        bool gotValidParameter = false;

        auto guid = m_parsedManifest.eventTypeByInternalName(eventName);
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
                    guid = m_parsedManifest.eventTypeByInternalName(
                        lit("%1.%2")
                            .arg(kVideoAnalytics)
                            .arg(videoAnalytics));

                    if (!guid.isNull())
                        result.push_back(guid);
                }
            }

            auto supportedLineAnalyticsParameter = parameters.parameter(
                "eventsources/videoanalysis/set/Line.#.Mode");

            gotValidParameter = supportedLineAnalyticsParameter.is_initialized()
                && supportedLineAnalyticsParameter->isValid();

            if (gotValidParameter)
            {
                guid = m_parsedManifest.eventTypeByInternalName(
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
                    guid = m_parsedManifest.eventTypeByInternalName(
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

const Hanwha::DriverManifest& HanwhaMetadataPlugin::manifest() const
{
    return m_parsedManifest;
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
