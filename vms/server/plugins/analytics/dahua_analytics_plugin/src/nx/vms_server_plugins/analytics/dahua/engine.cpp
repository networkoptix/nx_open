#include "engine.h"

#include "device_agent.h"
#include "common.h"
#include "string_helper.h"
#include "parser.h"

#include <chrono>

#include <QtCore/QString>
#include <QtCore/QUrlQuery>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <nx/network/http/http_client.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log_main.h>
#include <nx/sdk/helpers/string.h>

namespace nx::vms_server_plugins::analytics::dahua {

namespace {

const QString kVendor("dahua");
static const std::chrono::seconds kCacheTimeout{60};
static const std::chrono::seconds kRequestTimeout{10};

} // namespace

bool Engine::DeviceData::hasExpired() const
{
    return !timeout.isValid() || timeout.hasExpired(kCacheTimeout);
}

using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace {

const QString manifestFileName("plugins/dahua/manifest.json");
const QString manifestResourceName(":/dahua/manifest.json");

} // namespace

/*static*/ QByteArray Engine::loadManifest()
{
    QByteArray manifest;

    QFile file(manifestFileName);
    if (file.open(QFile::ReadOnly))
    {
        NX_INFO(NX_SCOPE_TAG,
            "Dahua engine manifest loaded from file %1", QFileInfo(file).absoluteFilePath());
        manifest = file.readAll();
        return manifest;
    }
    QFile resource(manifestResourceName);
    if (resource.open(QFile::ReadOnly))
    {
        NX_INFO(NX_SCOPE_TAG,
            "Dahua engine manifest loaded from resource %1", manifestResourceName);
        manifest = resource.readAll();
        return manifest;
    }

    NX_DEBUG(NX_SCOPE_TAG, "Dahua engine manifest failed to load from file %1 of from resource %2",
        manifestFileName, manifestResourceName);
    return manifest;
}

/*static*/ EngineManifest Engine::parseManifest(const QByteArray& manifest)
{
    bool success = false;
    const EngineManifest parsedManifest = QJson::deserialized<EngineManifest>(
        manifest, EngineManifest(), &success);
    if (!success)
        NX_WARNING(NX_SCOPE_TAG, "Can't deserialize Dahua engine manifest");
    return parsedManifest;
}

Engine::Engine(Plugin* plugin)
    :
    m_plugin(plugin),
    m_jsonManifest(loadManifest()),
    m_parsedManifest(parseManifest(m_jsonManifest))
{
}

void* Engine::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_Engine)
    {
        addRef();
        return static_cast<Engine*>(this);
    }
    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

void Engine::setSettings(const IStringMap* /*settings*/)
{
    // There are no DeviceAgent settings for this plugin.
}

IStringMap* Engine::pluginSideSettings() const
{
    return nullptr;
}

IDeviceAgent* Engine::obtainDeviceAgent(
    const IDeviceInfo* deviceInfo,
    Error* /*outError*/)
{
    if (!isCompatible(deviceInfo))
        return nullptr;

    const nx::vms::api::analytics::DeviceAgentManifest deviceAgentParsedManifest
        = fetchDeviceAgentParsedManifest(deviceInfo);
    if (deviceAgentParsedManifest.supportedEventTypeIds.isEmpty())
        return nullptr;

    return new DeviceAgent(this, deviceInfo, deviceAgentParsedManifest);
}

const nx::sdk::IString* Engine::manifest(Error* outError) const
{
    if (m_jsonManifest.isEmpty())
    {
        *outError = Error::unknownError;
        return nullptr;
    }

    *outError = Error::noError;
    return new nx::sdk::String(m_jsonManifest);
}

QList<QString> Engine::parseSupportedEvents(const QByteArray& data)
{
    QList<QString> result;
    auto supportedEvents = dahua::Parser::parseSupportedEventsMessage(data);
    if (!supportedEvents.empty())
    for (const auto& internalName: supportedEvents)
    {
        const QString eventTypeId = m_parsedManifest.eventTypeByInternalName(internalName);
        if (!eventTypeId.isEmpty())
        {
            result << eventTypeId;
            const auto descriptor = m_parsedManifest.eventTypeDescriptorById(eventTypeId);
            for (const auto& dependedName: descriptor.dependedEvent.split(','))
            {
                auto descriptor = m_parsedManifest.eventTypeDescriptorByInternalName(dependedName);
                if (!descriptor.id.isEmpty())
                    result << descriptor.id;
            }
        }
    }
    return result;
}

nx::vms::api::analytics::DeviceAgentManifest Engine::fetchDeviceAgentParsedManifest(
    const IDeviceInfo* deviceInfo)
{
    using namespace nx::vms::api::analytics;

    auto& data = m_cachedDeviceData[deviceInfo->sharedId()];
    if (!data.hasExpired())
        return DeviceAgentManifest{ data.supportedEventTypeIds };

    using namespace std::chrono;

    nx::utils::Url url(deviceInfo->url());
    url.setPath("/cgi-bin/eventManager.cgi");
    url.setQuery("action=getExposureEvents");

    nx::network::http::HttpClient httpClient;
    httpClient.setResponseReadTimeout(kRequestTimeout);
    httpClient.setSendTimeout(kRequestTimeout);
    httpClient.setMessageBodyReadTimeout(kRequestTimeout);
    httpClient.setUserName(deviceInfo->login());
    httpClient.setUserPassword(deviceInfo->password());

    const auto result = httpClient.doGet(url);
    const auto response = httpClient.response();

    if (!result || !response)
    {
        NX_WARNING(this, "No response for supported events request %1.", deviceInfo->url());
        data.timeout.invalidate();
        return DeviceAgentManifest{};
    }

    const auto statusCode = response->statusLine.statusCode;
    const auto buffer = httpClient.fetchEntireMessageBody();
    if (!nx::network::http::StatusCode::isSuccessCode(statusCode) || !buffer)
    {
        NX_WARNING(this, "Unable to fetch supported events for device %1. HTTP status code: %2",
            deviceInfo->url(), statusCode);
        data.timeout.invalidate();
        return DeviceAgentManifest{};
    }

    NX_DEBUG(this, "Device url %1. RAW list of supported analytics events: %2",
        deviceInfo->url(), buffer);

    data.supportedEventTypeIds = parseSupportedEvents(*buffer);
    data.timeout.restart();
    return nx::vms::api::analytics::DeviceAgentManifest{ data.supportedEventTypeIds };
}

const EngineManifest& Engine::parsedManifest() const
{
    return m_parsedManifest;
}

void Engine::executeAction(IAction* /*action*/, Error* /*outError*/)
{
}

Error Engine::setHandler(IHandler* /*handler*/)
{
    // TODO: Use the handler for error reporting.
    return Error::noError;
}

bool Engine::isCompatible(const IDeviceInfo* deviceInfo) const
{
    const auto vendor = QString(deviceInfo->vendor()).toLower();
    return vendor.startsWith(kVendor);
}

} // namespace nx::vms_server_plugins::analytics::dahua

namespace {

static const std::string kLibName = "dahua_analytics_plugin";

static const std::string kPluginManifest = /*suppress newline*/1 + R"json(
{
    "id": "nx.dahua",
    "name": "Dahua analytics plugin",
    "engineSettingsModel": ""
}
)json";

} // namespace

extern "C" {

NX_PLUGIN_API nxpl::PluginInterface* createNxPlugin()
{
    return new nx::sdk::analytics::Plugin(
        kLibName,
        kPluginManifest,
        [](nx::sdk::analytics::IPlugin* plugin)
        {
            return new nx::vms_server_plugins::analytics::dahua::Engine(
                dynamic_cast<nx::sdk::analytics::Plugin*>(plugin));
        });
}

} // extern "C"
