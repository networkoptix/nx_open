#include "device_agent.h"

#include <chrono>

#include <QtCore/QUrl>

#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/fusion/serialization/json.h>

#include <nx/sdk/helpers/string.h>
#include <nx/sdk/analytics/helpers/metadata_types.h>
#include <nx/sdk/helpers/settings_response.h>
#include <nx/sdk/helpers/plugin_diagnostic_event.h>
#include <nx/sdk/helpers/error.h>

#define NX_PRINT_PREFIX "[axis::DeviceAgent] "
#include <nx/kit/debug.h>

#include "common.h"

namespace nx::vms_server_plugins::analytics::axis {

using namespace nx::utils;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace {

/* 
 * Fence Guard event types are a special case. Their descriptions as reported by camera
 * contain arbitrary user-provided profile names. If we treat these generically, when
 * multiple cameras are attached that have differently named profiles in corresponding
 * profile slots, only one of these names ends up used by the VMS, which confuses users.
 *
 * Moreover, cameras report profiles in different slots as different event types, though
 * logically they are the same kind of event. There is also a special any-profile
 * event type, that is sent whenever any of the other profiles is sent.
 *
 * Thus we threat Fence Guard events specially:
 * 1) Any-profile event type is removed.
 * 2) All profile-specific event types together are translated to a single event type with
 * id nx.axis.FenceGuard, whose name no longer includes any profile-specific information.
 * 3) If the user still wishes to disambiguate between different profiles, the profile
 * name remains a part of the event's description. 
 */
EngineManifest mergeFenceGuardEventTypes(EngineManifest manifest)
{
    auto& eventTypes = manifest.eventTypes;
    for (auto it = eventTypes.begin(); it != eventTypes.end(); )
    {
        if (it->fenceGuardProfileIndex)
        {
            if (*it->fenceGuardProfileIndex == -1)
            {
                it->id = "nx.axis.FenceGuard";
                it->name = "Fence Guard";
                it->caption = "FenceGuard";
            }
            else
            {
                it = eventTypes.erase(it);
                continue;
            }
        }

        ++it;
    }

    return manifest;
}

Ptr<MetadataTypes> splitFenceGuardEvents(
    const IMetadataTypes& mergedMetadataTypes, const QList<EventType> eventTypes)
{
    auto metadataTypes = makePtr<MetadataTypes>();

    const auto objectTypeIds = mergedMetadataTypes.objectTypeIds();
    for (int i = 0; i < objectTypeIds->count(); ++i)
        metadataTypes->addObjectTypeId(objectTypeIds->at(i));

    const auto eventTypeIds = mergedMetadataTypes.eventTypeIds();
    for (int i = 0; i < eventTypeIds->count(); ++i)
    {
        QString id = eventTypeIds->at(i);
        if (id != "nx.axis.FenceGuard")
        {
            metadataTypes->addEventTypeId(id.toStdString());
            continue;
        }

        for (const auto& type: eventTypes)
        {
            if (type.fenceGuardProfileIndex)
                metadataTypes->addEventTypeId(type.id.toStdString());
        }
    }

    return metadataTypes;
}

} // namespace

DeviceAgent::DeviceAgent(
    Engine* engine,
    const IDeviceInfo* deviceInfo,
    const EngineManifest& typedManifest)
    :
    m_engine(engine),
    m_parsedManifest(typedManifest),
    m_jsonManifest(QJson::serialized(mergeFenceGuardEventTypes(typedManifest))),
    m_url(deviceInfo->url())
{
    m_auth.setUser(deviceInfo->login());
    m_auth.setPassword(deviceInfo->password());

    NX_PRINT << "Axis DeviceAgent created";
}

DeviceAgent::~DeviceAgent()
{
    stopFetchingMetadata();
    NX_PRINT << "Axis DeviceAgent destroyed";
}

void DeviceAgent::setHandler(IDeviceAgent::IHandler* handler)
{
    handler->addRef();
    m_handler.reset(handler);
}

void DeviceAgent::doSetNeededMetadataTypes(
    Result<void>* outResult, const IMetadataTypes* neededMetadataTypes)
{
    m_neededMetadataTypes = splitFenceGuardEvents(
        *neededMetadataTypes, m_parsedManifest.eventTypes);

    stopFetchingMetadata();
    *outResult = startFetchingMetadata();
}

void DeviceAgent::doSetSettings(
    Result<const ISettingsResponse*>* outResult, const IStringMap* settings)
{
    auto response = makePtr<SettingsResponse>();
    auto errors = makePtr<StringMap>();

    m_localMetadataPort = QString(settings->value("Network.Metadata.LocalPort")).toUInt();

    static const char kExternalAddress[] = "Network.Metadata.ExternalAddress";
    if (Url url = NX_FMT("//%1", settings->value(kExternalAddress)).toQString();
            url.isValid() && url.scheme().isEmpty() && url.userInfo().isEmpty() &&
            url.path().isEmpty() && !url.hasFragment() && !url.hasQuery())
    {
        if (url.port() == 0)
            url.setPort(-1);

        m_externalMetadataUrl = std::move(url);
    }
    else
    {
        errors->setItem(kExternalAddress,
            "Malformed; Should be either host:port, just host, or empty");
    }

    if (m_neededMetadataTypes)
    {
        stopFetchingMetadata();
        if (auto result = startFetchingMetadata(); !result.isOk())
        {
            *outResult = error(result.error().errorCode(), result.error().errorMessage()->str());
            return;
        }
    }

    response->setErrors(errors);
    *outResult = response.releasePtr();
}

void DeviceAgent::getPluginSideSettings(
    Result<const ISettingsResponse*>* /*outResult*/) const
{
}

Result<void> DeviceAgent::startFetchingMetadata()
{
    const auto eventTypeIds = m_neededMetadataTypes->eventTypeIds();
    if (const char* const kMessage = "Event type id list is null";
        !NX_ASSERT(eventTypeIds, kMessage))
    {
        return error(ErrorCode::internalError, kMessage);
    }

    if (eventTypeIds->count() == 0)
        return {};

    m_monitor = new Monitor(
        this, m_url, m_auth, m_localMetadataPort, m_externalMetadataUrl, m_handler.get());
    return m_monitor->startMonitoring(m_neededMetadataTypes.get());
}

void DeviceAgent::stopFetchingMetadata()
{
    delete m_monitor;
    m_monitor = nullptr;
}

void DeviceAgent::getManifest(Result<const IString*>* outResult) const
{
    if (m_jsonManifest.isEmpty())
        *outResult = error(ErrorCode::internalError, "DeviceAgent manifest is empty");
    else
        *outResult = new nx::sdk::String(m_jsonManifest);
}

const EventType* DeviceAgent::eventTypeById(const QString& id) const noexcept
{
    const auto it = std::find_if(
        m_parsedManifest.eventTypes.cbegin(),
        m_parsedManifest.eventTypes.cend(),
        [&id](const EventType& eventType) { return eventType.id == id; });

    if (it == m_parsedManifest.eventTypes.cend())
        return nullptr;
    else
        return &(*it);
}

void DeviceAgent::pushPluginDiagnosticEvent(
    IPluginDiagnosticEvent::Level level,
    std::string caption,
    std::string description)
{
    auto diagnosticEvent = makePtr<PluginDiagnosticEvent>(
        level,
        std::move(caption),
        std::move(description));

    m_handler->handlePluginDiagnosticEvent(diagnosticEvent.get());
}

} // nx::vms_server_plugins::analytics::axis
