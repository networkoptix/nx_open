#include "engine.h"

#include "device_agent.h"
#include "common.h"
#include "string_helper.h"
#include "attributes_parser.h"

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
namespace mediaserver_plugins {
namespace analytics {
namespace hikvision {

namespace {

const QString kHikvisionTechwinVendor = lit("hikvision");
static const std::chrono::seconds kCacheTimeout{60};

} // namespace

bool Engine::DeviceData::hasExpired() const
{
    return !timeout.isValid() || timeout.hasExpired(kCacheTimeout);
}

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(CommonPlugin* plugin): m_plugin(plugin)
{
    QFile file(":/hikvision/manifest.json");
    if (file.open(QFile::ReadOnly))
        m_manifest = file.readAll();
    {
        QFile file("plugins/hikvision/manifest.json");
        if (file.open(QFile::ReadOnly))
        {
            NX_INFO(this,
                lm("Switch to external manifest file %1").arg(QFileInfo(file).absoluteFilePath()));
            m_manifest = file.readAll();
        }
    }

    bool success = false;
    m_engineManifest = QJson::deserialized<Hikvision::EngineManifest>(
        m_manifest, Hikvision::EngineManifest(), &success);
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

void Engine::setSettings(const nx::sdk::Settings* settings)
{
    // There are no DeviceAgent settings for this plugin.
}

nx::sdk::Settings* Engine::pluginSideSettings() const
{
    return nullptr;
}

nx::sdk::analytics::DeviceAgent* Engine::obtainDeviceAgent(
    const DeviceInfo* deviceInfo,
    Error* outError)
{
    *outError = Error::noError;

    const auto vendor = QString(deviceInfo->vendor).toLower();

    if (!vendor.startsWith(kHikvisionTechwinVendor))
        return nullptr;

    auto supportedEventTypeIds = fetchSupportedEventTypeIds(*deviceInfo);
    if (!supportedEventTypeIds)
        return nullptr;

    nx::vms::api::analytics::DeviceAgentManifest deviceAgentManifest;
    deviceAgentManifest.supportedEventTypeIds = *supportedEventTypeIds;

    auto deviceAgent = new DeviceAgent(this);
    deviceAgent->setDeviceInfo(*deviceInfo);
    deviceAgent->setDeviceAgentManifest(QJson::serialized(deviceAgentManifest));
    deviceAgent->setEngineManifest(engineManifest());

    return deviceAgent;
}

const nx::sdk::IString* Engine::manifest(Error* error) const
{
    *error = Error::noError;
    return new common::String(m_manifest);
}

QList<QString> Engine::parseSupportedEvents(const QByteArray& data)
{
    QList<QString> result;
    auto supportedEvents = hikvision::AttributesParser::parseSupportedEventsXml(data);
    if (!supportedEvents)
        return result;
    for (const auto& internalName: *supportedEvents)
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

boost::optional<QList<QString>> Engine::fetchSupportedEventTypeIds(
    const DeviceInfo& deviceInfo)
{
    auto& data = m_cachedDeviceData[deviceInfo.sharedId];
    if (!data.hasExpired())
        return data.supportedEventTypeIds;

    nx::utils::Url url(deviceInfo.url);
    url.setPath("/ISAPI/Event/triggersCap");
    url.setUserInfo(deviceInfo.login);
    url.setPassword(deviceInfo.password);
    int statusCode = 0;
    QByteArray buffer;
    if (nx::network::http::downloadFileSync(url, &statusCode, &buffer) != SystemError::noError ||
        statusCode != nx::network::http::StatusCode::ok)
    {
        NX_WARNING(this,lm("Can't fetch supported events for device %1. HTTP status code: %2").
            arg(deviceInfo.url).arg(statusCode));
        return boost::optional<QList<QString>>();
    }
    NX_DEBUG(this, lm("Device url %1. RAW list of supported analytics events: %2").
        arg(deviceInfo.url).arg(buffer));

    data.supportedEventTypeIds = parseSupportedEvents(buffer);
    return data.supportedEventTypeIds;
}

const Hikvision::EngineManifest& Engine::engineManifest() const
{
    return m_engineManifest;
}

void Engine::executeAction(Action* /*action*/, Error* /*outError*/)
{
}

nx::sdk::Error Engine::setHandler(nx::sdk::analytics::Engine::IHandler* /*handler*/)
{
    // TODO: Use the handler for error reporting.
    return nx::sdk::Error::noError;
}

} // namespace hikvision
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx

namespace {

static const std::string kLibName = "hikvision_analytics_plugin";
static const std::string kPluginManifest = /*suppress newline*/1 + R"json(
{
    "id": "nx.hikvision",
    "name": "Hikvision analytics plugin",
    "engineSettingsModel": ""
}
)json";

} // namespace

extern "C" {

NX_PLUGIN_API nxpl::PluginInterface* createNxAnalyticsPlugin()
{
    return new nx::sdk::analytics::CommonPlugin(
        kLibName,
        kPluginManifest,
        [](nx::sdk::analytics::Plugin* plugin)
        {
            return new nx::mediaserver_plugins::analytics::hikvision::Engine(
                dynamic_cast<nx::sdk::analytics::CommonPlugin*>(plugin));
        });
}

} // extern "C"
