#include "hanwha_metadata_plugin.h"
#include "hanwha_metadata_manager.h"
#include "hanwha_common.h"
#include "hanwha_attributes_parser.h"
#include "hanwha_string_helper.h"

#include <QtCore/QString>
#include <QtCore/QUrlQuery>

#include <nx/network/http/http_client.h>

namespace nx {
namespace mediaserver {
namespace plugins {

namespace {

const char* kPluginName = "Hanwha metadata plugin";
const QString kSamsungTechwinVendor = lit("samsung techwin");
const QString kHanwhaTechwinVendor = lit("hanwha techwin");

const std::chrono::milliseconds kAttributesTimeout(4000);
const QString kAttributesPath = lit("/stw-cgi/attributes.cgi/eventstatus");

} // namespace

using namespace nx::sdk;
using namespace nx::sdk::metadata;

HanwhaMetadataPlugin::HanwhaMetadataPlugin()
{
    m_manifest = QString(kPluginManifestTemplate)
        .arg(str(kHanwhaFaceDetectionEventId))
        .arg(str(kHanwhaVirtualLineEventId))
        .arg(str(kHanwhaEnteringEventId))
        .arg(str(kHanwhaExitingEventId))
        .arg(str(kHanwhaAppearingEventId))
        .arg(str(kHanwhaDisappearingEventId))
        .arg(str(kHanwhaAudioDetectionEventId))
        .arg(str(kHanwhaTamperingEventId))
        .arg(str(kHanwhaDefocusingEventId))
        .arg(str(kHanwhaDryContactInputEventId))
        .arg(str(kHanwhaMotionDetectionEventId))
        .arg(str(kHanwhaSoundClassificationEventId))
        .arg(str(kHanwhaLoiteringEventId)).toUtf8();
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

    manager->setModel(resourceInfo.model);
    manager->setFirmware(resourceInfo.firmware);
    manager->setUrl(resourceInfo.url);
    manager->setAuth(auth);
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

boost::optional<std::vector<nxpl::NX_GUID>> HanwhaMetadataPlugin::fetchSupportedEvents(
    const QUrl& url,
    const QAuthenticator& auth)
{
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

    HanwhaAttributesParser parser;
    if (!parser.parseCgi(QString::fromUtf8(buffer)))
        return boost::none;

    auto supportedEvents = parser.supportedEvents();
    std::vector<nxpl::NX_GUID> result;

    for (const auto& eventName: supportedEvents)
    {
        auto guid = HanwhaStringHelper::fromStringToEventType(eventName);
        if (guid)
            result.push_back(*guid);
    }

    return result;
}

QUrl HanwhaMetadataPlugin::buildAttributesUrl(const QUrl& resourceUrl) const
{
    auto url = resourceUrl;
    url.setPath(kAttributesPath);
    url.setQuery(QUrlQuery());

    return url;
}

QByteArray HanwhaMetadataPlugin::buildDeviceManifest(
    const std::vector<nxpl::NX_GUID>& supportedEvents) const
{
    QString manifest = kDeviceManifestTemplate;
    QString supportedEventsStr;

    for (auto i = 0; i < supportedEvents.size(); ++i)
    {
        supportedEventsStr.append(lit("\"%1\"").arg(str(supportedEvents[i])));
        if (i < supportedEvents.size() - 1)
            supportedEventsStr.append(",\r\n");
    }

    return manifest.arg(supportedEventsStr).toUtf8();
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
