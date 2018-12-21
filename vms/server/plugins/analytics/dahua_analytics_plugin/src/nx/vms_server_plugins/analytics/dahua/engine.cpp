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
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log_main.h>
#include <nx/sdk/common/string.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace dahua {

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

Engine::Engine(nx::sdk::analytics::common::Plugin* plugin): m_plugin(plugin)
{
    QFile file(":/dahua/manifest.json");
    if (file.open(QFile::ReadOnly))
        m_manifest = file.readAll();
    {
        QFile file("plugins/dahua/manifest.json");
        if (file.open(QFile::ReadOnly))
        {
            NX_INFO(this,
                lm("Switch to external manifest file %1").arg(QFileInfo(file).absoluteFilePath()));
            m_manifest = file.readAll();
        }
    }

    bool success = false;
    m_engineManifest = QJson::deserialized<EngineManifest>(
        m_manifest, EngineManifest(), &success);
    if (!success)
        NX_WARNING(this, lm("Can't deserialize driver manifest file"));
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

void Engine::setSettings(const nx::sdk::IStringMap* settings)
{
    // There are no DeviceAgent settings for this plugin.
}

nx::sdk::IStringMap* Engine::pluginSideSettings() const
{
    return nullptr;
}

nx::sdk::analytics::IDeviceAgent* Engine::obtainDeviceAgent(
    const DeviceInfo* deviceInfo,
    Error* outError)
{
    *outError = Error::noError;

    if (!isCompatible(deviceInfo))
        return nullptr;

    auto supportedEventTypeIds = fetchSupportedEventTypeIds(*deviceInfo);
    if (supportedEventTypeIds.isEmpty())
        return nullptr;

    nx::vms::api::analytics::DeviceAgentManifest deviceAgentManifest;
    deviceAgentManifest.supportedEventTypeIds = std::move(supportedEventTypeIds);

    auto deviceAgent = new DeviceAgent(this);
    deviceAgent->setDeviceInfo(*deviceInfo);
    deviceAgent->setDeviceAgentManifest(QJson::serialized(deviceAgentManifest));
    deviceAgent->setEngineManifest(engineManifest());

    return deviceAgent;
}

const nx::sdk::IString* Engine::manifest(Error* error) const
{
    *error = Error::noError;
    return new nx::sdk::common::String(m_manifest);
}

QList<QString> Engine::parseSupportedEvents(const QByteArray& data)
{
    QList<QString> result;
    auto supportedEvents = dahua::Parser::parseSupportedEventsMessage(data);
    if (!supportedEvents.empty())
    for (const auto& internalName: supportedEvents)
    {
        const QString eventTypeId = m_engineManifest.eventTypeByInternalName(internalName);
        if (!eventTypeId.isEmpty())
        {
            result << eventTypeId;
            const auto descriptor = m_engineManifest.eventTypeDescriptorById(eventTypeId);
            for (const auto& dependedName: descriptor.dependedEvent.split(','))
            {
                auto descriptor = m_engineManifest.eventTypeDescriptorByInternalName(dependedName);
                if (!descriptor.id.isEmpty())
                    result << descriptor.id;
            }
        }
    }
    return result;
}

QList<QString> Engine::fetchSupportedEventTypeIds(const DeviceInfo& deviceInfo)
{
    auto& data = m_cachedDeviceData[deviceInfo.sharedId];
    if (!data.hasExpired())
        return data.supportedEventTypeIds;

    using namespace std::chrono;

    nx::utils::Url url(deviceInfo.url);
    url.setPath("/cgi-bin/eventManager.cgi");
    url.setQuery("action=getExposureEvents");

    nx::network::http::HttpClient httpClient;
    httpClient.setResponseReadTimeout(kRequestTimeout);
    httpClient.setSendTimeout(kRequestTimeout);
    httpClient.setMessageBodyReadTimeout(kRequestTimeout);
    httpClient.setUserName(deviceInfo.login);
    httpClient.setUserPassword(deviceInfo.password);

    const auto result = httpClient.doGet(url);
    const auto response = httpClient.response();

    if (!result || !response)
    {
        NX_WARNING(this, "No response for supported events request %1.", deviceInfo.url);
        data.timeout.invalidate();
        return QList<QString>();
    }

    const auto statusCode = response->statusLine.statusCode;
    const auto buffer = httpClient.fetchEntireMessageBody();
    if (!nx::network::http::StatusCode::isSuccessCode(statusCode) || !buffer)
    {
        NX_WARNING(this, "Unable to fetch supported events for device %1. HTTP status code: %2",
            deviceInfo.url, statusCode);
        data.timeout.invalidate();
        return QList<QString>();
    }

    NX_DEBUG(this, "Device url %1. RAW list of supported analytics events: %2",
        deviceInfo.url, buffer);

    data.supportedEventTypeIds = parseSupportedEvents(*buffer);
    data.timeout.restart();
    return data.supportedEventTypeIds;
}

const EngineManifest& Engine::engineManifest() const
{
    return m_engineManifest;
}

void Engine::executeAction(Action* /*action*/, Error* /*outError*/)
{
}

nx::sdk::Error Engine::setHandler(nx::sdk::analytics::IEngine::IHandler* /*handler*/)
{
    // TODO: Use the handler for error reporting.
    return nx::sdk::Error::noError;
}

bool Engine::isCompatible(const nx::sdk::DeviceInfo* deviceInfo) const
{
    const auto vendor = QString(deviceInfo->vendor).toLower();
    return vendor.startsWith(kVendor);
}

} // namespace dahua
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

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

NX_PLUGIN_API nxpl::PluginInterface* createNxAnalyticsPlugin()
{
    return new nx::sdk::analytics::common::Plugin(
        kLibName,
        kPluginManifest,
        [](nx::sdk::analytics::IPlugin* plugin)
        {
            return new nx::vms_server_plugins::analytics::dahua::Engine(
                dynamic_cast<nx::sdk::analytics::common::Plugin*>(plugin));
        });
}

} // extern "C"
