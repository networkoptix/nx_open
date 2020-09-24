#include "engine.h"

#include "device_agent.h"
#include "common.h"
#include "string_helper.h"
#include "attributes_parser.h"

#include <chrono>

#include <QtCore/QString>
#include <QtCore/QUrlQuery>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtXml/QDomElement>

#include <nx/network/http/http_client.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log_main.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/error.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace hikvision {

namespace {

const QString kHikvisionTechwinVendor = lit("hikvision");
static const std::chrono::seconds kCacheTimeout{60};
static const std::chrono::seconds kRequestTimeout{10};

} // namespace

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(Plugin* plugin): m_plugin(plugin)
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

void Engine::setEngineInfo(const nx::sdk::analytics::IEngineInfo* /*engineInfo*/)
{
}

void Engine::doSetSettings(
    Result<const ISettingsResponse*>* /*outResult*/, const IStringMap* /*settings*/)
{
    // There are no DeviceAgent settings for this plugin.
}

void Engine::getPluginSideSettings(Result<const ISettingsResponse*>* /*outResult*/) const
{
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    if (!isCompatible(deviceInfo))
        return;

    nx::vms::api::analytics::DeviceAgentManifest deviceAgentManifest;

    auto& deviceData = getCachedDeviceData(deviceInfo);
    if (!deviceData.timeout.isValid())
        return;

    deviceAgentManifest.supportedEventTypeIds = deviceData.supportedEventTypeIds;
    if (deviceAgentManifest.supportedEventTypeIds.isEmpty())
    {
        NX_DEBUG(this, "Supported Event Type list is empty for the Device %1 (%2)",
            deviceInfo->name(), deviceInfo->id());
    }

    deviceAgentManifest.supportedObjectTypeIds = deviceData.supportedObjectTypeIds;
    if (deviceAgentManifest.supportedObjectTypeIds.isEmpty())
    {
        NX_DEBUG(this, "Supported Object Type list is empty for the Device %1 (%2)",
            deviceInfo->name(), deviceInfo->id());
    }

    const auto deviceAgent = new DeviceAgent(this);
    deviceAgent->setDeviceInfo(deviceInfo);
    deviceAgent->setDeviceAgentManifest(QJson::serialized(deviceAgentManifest));
    deviceAgent->setEngineManifest(engineManifest());

    *outResult = deviceAgent;
}

void Engine::getManifest(Result<const IString*>* outResult) const
{
    *outResult = new nx::sdk::String(m_manifest);
}

QList<QString> Engine::parseSupportedEvents(const QByteArray& data)
{
    QList<QString> result;
    auto supportedEvents = hikvision::AttributesParser::parseSupportedEventsXml(data);
    if (!supportedEvents)
        return result;
    for (const auto& internalName: *supportedEvents)
    {
        const QString eventTypeId = m_engineManifest.eventTypeIdByInternalName(internalName);
        if (!eventTypeId.isEmpty())
        {
            result << eventTypeId;
            const Hikvision::EventType eventType = m_engineManifest.eventTypeById(eventTypeId);
            for (const auto& dependentName: eventType.dependedEvent.split(','))
            {
                const Hikvision::EventType dependentEventType =
                    m_engineManifest.eventTypeByInternalName(dependentName);

                if (!dependentEventType.id.isEmpty())
                    result << dependentEventType.id;
            }
        }
    }
    const QSet unique = QSet<QString>::fromList(result);
    result = unique.toList();
    return result;
}

bool Engine::fetchSupportedEventTypeIds(DeviceData* deviceData, const IDeviceInfo* deviceInfo)
{
    using namespace std::chrono;

    nx::utils::Url url(deviceInfo->url());
    url.setPath("/ISAPI/Event/triggersCap");

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
        NX_WARNING(
            this,
            lm("No response for supported events request %1.").args(deviceInfo->url()));
        return false;
    }

    const auto statusCode = response->statusLine.statusCode;
    const auto buffer = httpClient.fetchEntireMessageBody();
    if (!nx::network::http::StatusCode::isSuccessCode(statusCode) || !buffer)
    {
        NX_WARNING(
            this,
            lm("Unable to fetch supported events for device %1. HTTP status code: %2")
                .args(deviceInfo->url(), statusCode));
        return false;
    }

    NX_DEBUG(this, lm("Device url %1. RAW list of supported analytics events: %2").
        args(deviceInfo->url(), buffer));

    deviceData->supportedEventTypeIds = parseSupportedEvents(*buffer);

    return true;
}

std::optional<QList<QString>> Engine::parseSupportedObjects(const QByteArray& data)
{
    QDomDocument document;
    {
        QString errorDescription;
        int errorLine, errorColumn;
        if (!document.setContent(data, false, &errorDescription, &errorLine, &errorColumn))
        {
            NX_DEBUG(NX_SCOPE_TAG, "Failed to parse XML at %1:%2: %3",
                errorLine, errorColumn, errorDescription);
            return std::nullopt;
        }
    }

    const auto metadataCfg = document.documentElement();
    if (metadataCfg.tagName() != "MetadataCfg")
    {
        NX_DEBUG(NX_SCOPE_TAG, "XML root element is not 'MetadataCfg'");
        return std::nullopt;
    }

    const auto metadataList = metadataCfg.firstChildElement("MetadataList");
    if (metadataList.isNull())
    {
        NX_DEBUG(NX_SCOPE_TAG, "No 'MetadataList' XML element in 'MetadataCfg' element");
        return std::nullopt;
    }

    QSet<QString> capabilities;
    for (auto metadata = metadataList.firstChildElement("Metadata"); !metadata.isNull();
        metadata = metadata.nextSiblingElement("Metadata"))
    {
        const auto enable = metadata.firstChildElement("enable");
        if (enable.isNull())
        {
            NX_DEBUG(NX_SCOPE_TAG, "No 'enable' XML element in 'Metadata' element");
            continue;
        }
        if (enable.text() != "true")
            continue;

        const auto type = metadata.firstChildElement("type");
        if (type.isNull())
        {
            NX_DEBUG(NX_SCOPE_TAG, "No 'type' XML element in 'Metadata' element");
            continue;
        }
        capabilities.insert(type.text());
    }

    QList<QString> typeIds;
    for (const auto& objectType: m_engineManifest.objectTypes)
    {
        if (objectType.sourceCapabilities.intersects(capabilities))
            typeIds.push_back(objectType.id);
    }
    return typeIds;
}

bool Engine::fetchSupportedObjectTypeIds(DeviceData* deviceData, const IDeviceInfo* deviceInfo)
{
    nx::utils::Url url(deviceInfo->url());
    url.setPath(NX_FMT("/ISAPI/Streaming/channels/%1/metadata/capabilities",
        1 + deviceInfo->channelNumber()));

    nx::network::http::HttpClient httpClient;
    httpClient.setResponseReadTimeout(kRequestTimeout);
    httpClient.setSendTimeout(kRequestTimeout);
    httpClient.setMessageBodyReadTimeout(kRequestTimeout);
    httpClient.setUserName(deviceInfo->login());
    httpClient.setUserPassword(deviceInfo->password());

    httpClient.doGet(url);
    if (!httpClient.hasRequestSucceeded())
    {
        NX_DEBUG(this, "No response for supported objects request %1", deviceInfo->url());
        return false;
    }

    const auto response = httpClient.response();
    const auto statusCode = response->statusLine.statusCode;
    const auto buffer = httpClient.fetchEntireMessageBody();
    if (!nx::network::http::StatusCode::isSuccessCode(statusCode) || !buffer)
    {
        NX_DEBUG(this, "Unable to fetch supported objects for device %1. HTTP status code: %2",
            deviceInfo->url(), statusCode);
        return false;
    }

    if (auto typeIds = parseSupportedObjects(*buffer))
    {
        deviceData->supportedObjectTypeIds = std::move(*typeIds);
    }
    else
    {
        NX_DEBUG(this, "Failed to parse list of objects supported by device %1. Raw XML: %2",
            deviceInfo->url(), *buffer);
        return false;
    }

    return true;
}

Engine::DeviceData& Engine::getCachedDeviceData(const IDeviceInfo* deviceInfo)
{
    auto& data = m_cachedDeviceData[deviceInfo->sharedId()];
    auto& timeout = data.timeout;
    if (!timeout.isValid() || timeout.hasExpired(kCacheTimeout))
    {
        timeout.invalidate();

        const bool eventTypesFetched = fetchSupportedEventTypeIds(&data, deviceInfo);
        const bool objectTypesFetched = fetchSupportedObjectTypeIds(&data, deviceInfo);
        if (eventTypesFetched || objectTypesFetched)
        {
            static const char eventObjectTypeId[] = "nx.hikvision.event";
            if (!data.supportedEventTypeIds.isEmpty()
                && !data.supportedObjectTypeIds.contains(eventObjectTypeId))
            {
                data.supportedObjectTypeIds.push_back(eventObjectTypeId);
            }

            timeout.restart();
        }
    }
    return data;
}

const Hikvision::EngineManifest& Engine::engineManifest() const
{
    return m_engineManifest;
}

void Engine::doExecuteAction(Result<IAction::Result>* /*outResult*/, const IAction* /*action*/)
{
}

void Engine::setHandler(IHandler* /*handler*/)
{
    // TODO: Use the handler for error reporting.
}

bool Engine::isCompatible(const IDeviceInfo* deviceInfo) const
{
    const auto vendor = QString(deviceInfo->vendor()).toLower();
    return vendor.startsWith(kHikvisionTechwinVendor);
}

} // namespace hikvision
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

namespace {

static const std::string kPluginManifest = /*suppress newline*/ 1 + (const char*) R"json(
{
    "id": "nx.hikvision",
    "name": "Hikvision analytics plugin",
    "description": "Supports built-in analytics on Hikvision cameras",
    "version": "1.0.0"
}
)json";

} // namespace

extern "C" {

NX_PLUGIN_API nx::sdk::IPlugin* createNxPlugin()
{
    return new nx::sdk::analytics::Plugin(
        kPluginManifest,
        [](nx::sdk::analytics::IPlugin* plugin)
        {
            return new nx::vms_server_plugins::analytics::hikvision::Engine(
                dynamic_cast<nx::sdk::analytics::Plugin*>(plugin));
        });
}

} // extern "C"
