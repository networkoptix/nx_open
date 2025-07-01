// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>

#include <QtCore/QDateTime>
#include <QtNetwork/QHostAddress>

#include <nx/vms/api/rules/event_log.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/rules/aggregated_event.h>
#include <nx/vms/rules/events/builtin_events.h>
#include <nx/vms/rules/manifest.h>
#include <nx/vms/rules/utils/event_log.h>

#include "event_details_test_base.h"

using namespace std::chrono;

namespace nx::vms::rules {

void PrintTo(ResourceType value, std::ostream* os) { *os << nx::reflect::toString(value); }

} // namespace nx::vms::rules

namespace nx::vms::rules::test {

namespace {

/** Return timestamp from time like: "15:46:29" with an example date. */
std::chrono::milliseconds makeTimestamp(const QString& time)
{
    static const QString kTimeFormat = "hh:mm:ss";
    static const QDate kDate(2025, 10, 25); //< Date is not displayed in notifications.

    const QDateTime dt(kDate, QTime::fromString(time, kTimeFormat));
    return std::chrono::milliseconds(dt.toMSecsSinceEpoch());
}

} // namespace

class EventLogTest: public EventDetailsTestBase
{
    using EventLog = utils::EventLog;

protected:
    AggregatedEventPtr buildLogEvent()
    {
        if (!m_logEvent)
        {
            nx::vms::api::rules::EventLogRecord record;
            buildEvent()->storeToRecord(&record);
            m_logEvent = AggregatedEventPtr::create(systemContext()->vmsRulesEngine(), record);
        }

        return m_logEvent;
    }

    void thenSourceIs(
        const QString& expectedText,
        std::pair<ResourceType, bool /*plural*/> expectedIcon,
        const UuidList& expectedResourceIds)
    {
        auto event = buildLogEvent();

        const auto resourceIds = EventLog::sourceResourceIds(event, systemContext());

        EXPECT_EQ(expectedText, EventLog::sourceText(event, systemContext(), Qn::RI_WithUrl));
        EXPECT_EQ(expectedIcon, EventLog::sourceIcon(event, systemContext()));
        EXPECT_EQ(expectedResourceIds, resourceIds);
    }

    void thenSourceIs(
        const QString& expectedText,
        ResourceType expectedIcon,
        const Uuid& expectedResourceId)
    {
        thenSourceIs(
            expectedText,
            std::make_pair(expectedIcon, /*plural*/ false),
            UuidList{expectedResourceId});
    }

    void thenDescriptionTooltipIs(const QString& expectedText)
    {
        auto event = buildLogEvent();
        EXPECT_EQ(expectedText.trimmed(),
            EventLog::descriptionTooltip(event, systemContext(), Qn::RI_WithUrl));
    }

private:
    AggregatedEventPtr m_logEvent;
};

// FIXME: #sivanov Short plan:
// - Add analytics engines to sources of the analytics events.
// - Support multiple cameras in Generic / Soft Trigger events.
// - List cameras in Saas Issue / Server Conflict / Device IP Conflict / License Issue events.
// - Decide what to do with Generic Events with different sources.
// - Support removed devices.

//-------------------------------------------------------------------------------------------------
// Content tests

TEST_F(EventLogTest, content_eventsPerDayLimit)
{
    static constexpr auto kExpectedDescriptionTooltip = R"(
Server1 (localhost) Started
15:46:01
15:46:02
...
15:46:59
15:47:00

Total number of events: 60
)";

    auto timestamp = makeTimestamp("15:46:01");
    for (int i = 0; i < 60; ++i)
        givenEvent(QSharedPointer<ServerStartedEvent>::create(timestamp + (1s * i), kServerId));

    whenEventsLimitSetTo(4);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

TEST_F(EventLogTest, content_singleEventMigration)
{
    static constexpr auto kExpectedDescriptionTooltip = R"(
Server1 (localhost) Started
15:46:01
...

Total number of events: 60
)";

    auto timestamp = makeTimestamp("15:46:01");
    for (int i = 0; i < 60; ++i)
        givenEvent(QSharedPointer<ServerStartedEvent>::create(timestamp + (1s * i), kServerId));

    whenEventsLimitSetTo(1);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

TEST_F(EventLogTest, content_removedSource)
{
    static constexpr auto kExpectedSourceText = "Removed server";
    static constexpr auto kExpectedSourceIcon = ResourceType::server;

    const auto removedServerId = Uuid::createUuid();

    givenEvent(QSharedPointer<ServerStartedEvent>::create(
        makeTimestamp("15:46:29"),
        removedServerId));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, removedServerId);
}

//-------------------------------------------------------------------------------------------------
// Analytics Event

TEST_F(EventLogTest, event_analytics)
{
    static const nx::Uuid kEngine2Id = nx::Uuid::createUuid();
    static const QString kEventTypeId = "nx.LineCrossing";

    static constexpr auto kExpectedSourceText = "Entrance (10.0.0.1)";
    static constexpr auto kExpectedSourceIcon = ResourceType::device;
    static constexpr auto kExpectedDescriptionTooltip = R"(
Analytics Event at Entrance (10.0.0.1)
15:46:29
  Caption: Caption 1
  Description: Description 1
15:48:30
  Caption: Caption 2
  Description: Description 2
)";

    givenEvent(QSharedPointer<AnalyticsEvent>::create(
        makeTimestamp("15:46:29"),
        State::started,
        "Caption 1",
        "Description 1",
        kCamera1Id,
        kEngine1Id,
        kEventTypeId,
        nx::common::metadata::Attributes(),
        /*objectTrackId*/ nx::Uuid::createUuid(),
        /*key*/ QString{},
        /*boundingBox*/ QRectF{}
    ));

    givenEvent(QSharedPointer<AnalyticsEvent>::create(
        makeTimestamp("15:48:30"),
        State::started,
        "Caption 2",
        "Description 2",
        kCamera1Id,
        kEngine2Id,
        kEventTypeId, //< Event type is fixed in UI.
        nx::common::metadata::Attributes(),
        /*objectTrackId*/ nx::Uuid::createUuid(),
        /*key*/ QString{},
        /*boundingBox*/ QRectF{}
    ));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, kCamera1Id);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

//-------------------------------------------------------------------------------------------------
// Analytics Object

TEST_F(EventLogTest, event_analyticsObject)
{
    static const nx::Uuid kEngine2Id = nx::Uuid::createUuid();
    static const QString kObjectTypeId1 = "nx.LicensePlate";
    static const QString kObjectTypeId2 = "nx.Face";

    static constexpr auto kExpectedSourceText = "Entrance (10.0.0.1)";
    static constexpr auto kExpectedSourceIcon = ResourceType::device;
    static constexpr auto kExpectedDescriptionTooltip = R"(
Object detected at Entrance (10.0.0.1)
15:46:29
15:48:30
)";

    givenEvent(QSharedPointer<AnalyticsObjectEvent>::create(
        State::started,
        makeTimestamp("15:46:29"),
        kCamera1Id,
        kEngine1Id,
        kObjectTypeId1,
        /*objectTrackId*/ nx::Uuid::createUuid(),
        nx::common::metadata::Attributes()
    ));

    givenEvent(QSharedPointer<AnalyticsObjectEvent>::create(
        State::started,
        makeTimestamp("15:48:30"),
        kCamera1Id,
        kEngine2Id,
        kObjectTypeId2, //< TODO: sivanov Probably fixed in UI.
        /*objectTrackId*/ nx::Uuid::createUuid(),
        nx::common::metadata::Attributes()
    ));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, kCamera1Id);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

//-------------------------------------------------------------------------------------------------
// Camera Input

TEST_F(EventLogTest, event_cameraInput)
{
    static constexpr auto kExpectedSourceText = "Entrance (10.0.0.1)";
    static constexpr auto kExpectedSourceIcon = ResourceType::device;
    static constexpr auto kExpectedDescriptionTooltip = R"(
Input on Entrance (10.0.0.1)
15:46:29
  Input Port: Port_1
15:48:30
  Input Port: Port_2
)";

    givenEvent(QSharedPointer<CameraInputEvent>::create(
        makeTimestamp("15:46:29"),
        State::started,
        kCamera1Id,
        "Port_1"
    ));

    givenEvent(QSharedPointer<CameraInputEvent>::create(
        makeTimestamp("15:48:30"),
        State::started,
        kCamera1Id,
        "Port_2"
    ));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, kCamera1Id);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

//-------------------------------------------------------------------------------------------------
// Device Disconnected

TEST_F(EventLogTest, event_deviceDisconnected)
{
    static constexpr auto kExpectedSourceText = "Entrance (10.0.0.1)";
    static constexpr auto kExpectedSourceIcon = ResourceType::device;
    static constexpr auto kExpectedDescriptionTooltip = R"(
Camera disconnected at Server1 (localhost)
15:46:29
  Entrance (10.0.0.1)
15:48:30
  Camera2 (10.0.0.2)
)";

    givenEvent(QSharedPointer<DeviceDisconnectedEvent>::create(
        makeTimestamp("15:46:29"),
        kCamera1Id,
        kServerId
    ));

    givenEvent(QSharedPointer<DeviceDisconnectedEvent>::create(
        makeTimestamp("15:48:30"),
        kCamera2Id,
        kServerId
    ));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, kCamera1Id);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

//-------------------------------------------------------------------------------------------------
// Device IP Conflict

TEST_F(EventLogTest, event_deviceIpConflict)
{
    static constexpr auto kExpectedSourceText = "Server1 (localhost)";
    static constexpr auto kExpectedSourceIcon = ResourceType::server;
    static constexpr auto kExpectedDescriptionTooltip = R"(
Camera IP Conflict at Server1 (localhost)
15:46:29
  Conflicting Address: 10.0.0.1
  Camera #1: Entrance (2a:cf:c7:04:c7:c9)
  Camera #2: Camera2 (51:dc:54:02:3a:8e)
15:48:30
  Conflicting Address: 10.0.0.2
  Camera #1: Entrance (2a:cf:c7:04:c7:c9)
  Camera #2: Camera2 (51:dc:54:02:3a:8e)
)";

    givenEvent(QSharedPointer<DeviceIpConflictEvent>::create(
        makeTimestamp("15:46:29"),
        kServerId,
        QList<DeviceIpConflictEvent::DeviceInfo>{
            {.id = kCamera1Id, .mac = kCamera1PhysicalId},
            {.id = kCamera2Id, .mac = kCamera2PhysicalId}},
        /*address*/ QHostAddress("10.0.0.1")));

    givenEvent(QSharedPointer<DeviceIpConflictEvent>::create(
        makeTimestamp("15:48:30"),
        kServerId,
        QList<DeviceIpConflictEvent::DeviceInfo>{
            {.id = kCamera1Id, .mac = kCamera1PhysicalId},
            {.id = kCamera2Id, .mac = kCamera2PhysicalId}},
        /*address*/ QHostAddress("10.0.0.2")));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, kServerId);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

//-------------------------------------------------------------------------------------------------
// Fan Error

TEST_F(EventLogTest, event_fanError)
{
    static constexpr auto kExpectedSourceText = "Server1 (localhost)";
    static constexpr auto kExpectedSourceIcon = ResourceType::server;
    static constexpr auto kExpectedDescriptionTooltip = R"(
Fan failure at Server1 (localhost)
15:46:29
15:48:30
)";

    givenEvent(QSharedPointer<FanErrorEvent>::create(
        makeTimestamp("15:46:29"),
        kServerId));

    givenEvent(QSharedPointer<FanErrorEvent>::create(
        makeTimestamp("15:48:30"),
        kServerId));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, kServerId);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

//-------------------------------------------------------------------------------------------------
// Generic Event

TEST_F(EventLogTest, event_generic_withSourceOnly)
{
    static constexpr auto kExpectedSourceText = "Source 1";
    static constexpr auto kExpectedSourceIcon = ResourceType::server;
    static const Uuid kExpectedResourceId = kServerId;
    static constexpr auto kExpectedDescriptionTooltip = R"(
Generic Event at Server1 (localhost)
15:46:29
  Source: Source 1
  Caption: Caption 1
  Description: Description 1
)";

    givenEvent(QSharedPointer<GenericEvent>::create(
        makeTimestamp("15:46:29"),
        State::instant,
        /*caption*/ "Caption 1",
        /*description*/ "Description 1",
        /*source*/ "Source 1",
        kServerId,
        UuidList{}));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, kExpectedResourceId);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

TEST_F(EventLogTest, event_generic_withSourceAndDevice)
{
    static constexpr auto kExpectedSourceText = "Source 1";
    static constexpr auto kExpectedSourceIcon = ResourceType::device;
    static const Uuid kExpectedResourceId = kCamera1Id;
    static constexpr auto kExpectedDescriptionTooltip = R"(
Generic Event at Server1 (localhost)
15:46:29
  Source: Source 1
  Caption: Caption 1
  Description: Description 1
  Related cameras:
  - Entrance (10.0.0.1)
)";

    givenEvent(QSharedPointer<GenericEvent>::create(
        makeTimestamp("15:46:29"),
        State::instant,
        /*caption*/ "Caption 1",
        /*description*/ "Description 1",
        /*source*/ "Source 1",
        kServerId,
        UuidList{kCamera1Id}));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, kExpectedResourceId);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

TEST_F(EventLogTest, event_generic_withDevicesOnly)
{
    static constexpr auto kExpectedSourceText = "Entrance (10.0.0.1)";
    static constexpr auto kExpectedSourceIcon = ResourceType::device;
    static const Uuid kExpectedResourceId = kCamera1Id;
    static constexpr auto kExpectedDescriptionTooltip = R"(
Generic Event at Server1 (localhost)
15:46:29
  Caption: Caption 1
  Description: Description 1
  Related cameras:
  - Entrance (10.0.0.1)
)";

    givenEvent(QSharedPointer<GenericEvent>::create(
        makeTimestamp("15:46:29"),
        State::instant,
        /*caption*/ "Caption 1",
        /*description*/ "Description 1",
        /*source*/ QString(),
        kServerId,
        UuidList{kCamera1Id}));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, kExpectedResourceId);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

TEST_F(EventLogTest, event_generic_withServerOnly)
{
    static constexpr auto kExpectedSourceText = "Server1 (localhost)";
    static constexpr auto kExpectedSourceIcon = ResourceType::server;
    static const Uuid kExpectedResourceId = kServerId;
    static constexpr auto kExpectedDescriptionTooltip = R"(
Generic Event at Server1 (localhost)
15:46:29
  Caption: Caption 1
  Description: Description 1
)";

    givenEvent(QSharedPointer<GenericEvent>::create(
        makeTimestamp("15:46:29"),
        State::instant,
        /*caption*/ "Caption 1",
        /*description*/ "Description 1",
        /*source*/ QString(),
        kServerId,
        UuidList{}));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, kExpectedResourceId);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

//-------------------------------------------------------------------------------------------------
// LDAP Sync Issue

TEST_F(EventLogTest, event_ldapSyncIssue)
{
    static constexpr auto kExpectedSourceText = "Server1 (localhost)";
    static constexpr auto kExpectedSourceIcon = ResourceType::server;
    static constexpr auto kExpectedDescriptionTooltip = R"(
LDAP Sync Issue at Server1 (localhost)
15:46:29
  Failed to complete the sync within a 5 minutes timeout.
15:48:30
  Failed to connect to the LDAP server.
)";

    givenEvent(QSharedPointer<LdapSyncIssueEvent>::create(
        makeTimestamp("15:46:29"),
        nx::vms::api::EventReason::failedToCompleteSyncWithLdap,
        5min,
        kServerId));

    givenEvent(QSharedPointer<LdapSyncIssueEvent>::create(
        makeTimestamp("15:48:30"),
        nx::vms::api::EventReason::failedToConnectToLdap,
        30s,
        kServerId));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, kServerId);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

//-------------------------------------------------------------------------------------------------
// License Issue

TEST_F(EventLogTest, event_licenseIssue)
{
    static constexpr auto kExpectedSourceText = "Server1 (localhost)";
    static constexpr auto kExpectedSourceIcon = ResourceType::server;
    static constexpr auto kExpectedDescriptionTooltip = R"(
Not enough licenses on Server1 (localhost)
15:46:29
  Recording has been disabled on the following cameras:
  - Entrance (10.0.0.1)
15:48:30
  Recording has been disabled on the following cameras:
  - Camera2 (10.0.0.2)
  - Entrance (10.0.0.1)
)";

    givenEvent(QSharedPointer<LicenseIssueEvent>::create(
        makeTimestamp("15:46:29"),
        kServerId,
        /*disabledCameras*/ UuidSet{kCamera1Id}));

    givenEvent(QSharedPointer<LicenseIssueEvent>::create(
        makeTimestamp("15:48:30"),
        kServerId,
        /*disabledCameras*/ UuidSet{kCamera1Id, kCamera2Id}));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, kServerId);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

//-------------------------------------------------------------------------------------------------
// Camera Motion

TEST_F(EventLogTest, event_motion)
{
    static constexpr auto kExpectedSourceText = "Entrance (10.0.0.1)";
    static constexpr auto kExpectedSourceIcon = ResourceType::device;
    static constexpr auto kExpectedDescriptionTooltip = R"(
Motion on Entrance (10.0.0.1)
15:46:29
15:48:30
)";

    givenEvent(QSharedPointer<MotionEvent>::create(
        makeTimestamp("15:46:29"),
        State::started,
        kCamera1Id
    ));
    givenEvent(QSharedPointer<MotionEvent>::create(
        makeTimestamp("15:48:30"),
        State::started,
        kCamera1Id
    ));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, kCamera1Id);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

//-------------------------------------------------------------------------------------------------
// Network Issue

TEST_F(EventLogTest, event_networkIssue)
{
    static constexpr auto kExpectedSourceText = "Entrance (10.0.0.1)";
    static constexpr auto kExpectedSourceIcon = ResourceType::device;
    static constexpr auto kExpectedDescriptionTooltip = R"(
Network Issue at Server1 (localhost)
15:46:29
  Entrance (10.0.0.1)
  Multicast address conflict detected. Address 10.0.0.1:5555 is already in use by testcamera on secondary stream.
15:48:30
  Camera2 (10.0.0.2)
  No data received during last 5 seconds.
)";

    givenEvent(QSharedPointer<NetworkIssueEvent>::create(
        makeTimestamp("15:46:29"),
        kCamera1Id,
        kServerId,
        nx::vms::api::EventReason::networkMulticastAddressConflict,
        NetworkIssueInfo{
            .address = "10.0.0.1:5555",
            .deviceName = "testcamera",
            .stream = nx::vms::api::StreamIndex::secondary
        }));

    givenEvent(QSharedPointer<NetworkIssueEvent>::create(
        makeTimestamp("15:48:30"),
        kCamera2Id,
        kServerId,
        nx::vms::api::EventReason::networkNoFrame,
        NetworkIssueInfo{
            .timeout = 5s
        }));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, kCamera1Id);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

//-------------------------------------------------------------------------------------------------
// Integration Diagnostic

TEST_F(EventLogTest, event_integrationDiagnostic_deviceAgent)
{
    static constexpr auto kExpectedSourceText = "Entrance (10.0.0.1)";
    static constexpr auto kExpectedSourceIcon = ResourceType::device;
    static constexpr auto kExpectedDescriptionTooltip = R"(
Integration Diagnostic at Entrance (10.0.0.1)
15:46:29
  Caption: Caption 1
15:48:30
  Description: Description 2
)";

    givenEvent(QSharedPointer<IntegrationDiagnosticEvent>::create(
        makeTimestamp("15:46:29"),
        "Caption 1",
        /*description*/ QString(),
        kCamera1Id,
        kEngine1Id,
        nx::vms::api::EventLevel::error
    ));

    givenEvent(QSharedPointer<IntegrationDiagnosticEvent>::create(
        makeTimestamp("15:48:30"),
        /*caption*/ QString(),
        "Description 2",
        kCamera1Id,
        kEngine1Id, //< Engine is fixed in UI.
        nx::vms::api::EventLevel::info
    ));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, kCamera1Id);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

TEST_F(EventLogTest, event_integrationDiagnostic_plugin)
{
    static constexpr auto kExpectedSourceText = "Engine1";
    static constexpr auto kExpectedSourceIcon = ResourceType::analyticsEngine;
    static const Uuid kExpectedResourceId = kEngine1Id;
    static constexpr auto kExpectedDescriptionTooltip = R"(
Integration Diagnostic at Engine1
15:46:29
  Caption: Caption 1
15:48:30
  Description: Description 2
)";

    givenEvent(QSharedPointer<IntegrationDiagnosticEvent>::create(
        makeTimestamp("15:46:29"),
        "Caption 1",
        /*description*/ QString(),
        /*deviceId*/ nx::Uuid(),
        kEngine1Id,
        nx::vms::api::EventLevel::error
    ));

    givenEvent(QSharedPointer<IntegrationDiagnosticEvent>::create(
        makeTimestamp("15:48:30"),
        /*caption*/ QString(),
        "Description 2",
        /*deviceId*/ nx::Uuid(),
        kEngine1Id, //< Engine is fixed in UI.
        nx::vms::api::EventLevel::info
    ));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, kExpectedResourceId);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

//-------------------------------------------------------------------------------------------------
// POE Over Budget

TEST_F(EventLogTest, event_poeOverBudget)
{
    static constexpr auto kExpectedSourceText = "Server1 (localhost)";
    static constexpr auto kExpectedSourceIcon = ResourceType::server;
    static constexpr auto kExpectedDescriptionTooltip = R"(
PoE over budget on Server1 (localhost)
15:46:29
  Current power consumption: 0.5 watts
  Upper consumption limit: 0.3 watts
  Lower consumption limit: 0.2 watts
15:48:30
  Current power consumption: 0.8 watts
  Upper consumption limit: 0.2 watts
  Lower consumption limit: 0.1 watts
)";

    givenEvent(QSharedPointer<PoeOverBudgetEvent>::create(
        makeTimestamp("15:46:29"),
        State::instant,
        kServerId,
        /*currentConsumptionW*/ 0.52,
        /*upperLimitW*/ 0.35,
        /*lowerLimitW*/ 0.18));

    givenEvent(QSharedPointer<PoeOverBudgetEvent>::create(
        makeTimestamp("15:48:30"),
        State::instant,
        kServerId,
        /*currentConsumptionW*/ 0.85,
        /*upperLimitW*/ 0.2,
        /*lowerLimitW*/ 0.1));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, kServerId);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

//-------------------------------------------------------------------------------------------------
// SaaS Issue

TEST_F(EventLogTest, event_saasIssue)
{
    static constexpr auto kExpectedSourceText = "Server1 (localhost)";
    static constexpr auto kExpectedSourceIcon = ResourceType::server;
    static constexpr auto kExpectedDescriptionTooltip = R"(
Services Issue on Server1 (localhost)
15:46:29
  Failed to migrate licenses.
  - key1
  - key2
15:48:30
  Paid integration service usage on 2 channels was stopped due to service overuse.
  - Entrance (10.0.0.1)
  - Camera2 (10.0.0.2)
)";

    givenEvent(QSharedPointer<SaasIssueEvent>::create(
        makeTimestamp("15:46:29"),
        kServerId,
        /*licenseKeys*/ QStringList{"key1", "key2"},
        nx::vms::api::EventReason::licenseMigrationFailed));

    givenEvent(QSharedPointer<SaasIssueEvent>::create(
        makeTimestamp("15:48:30"),
        kServerId,
        /*deviceIds*/ UuidList{kCamera1Id, kCamera2Id},
        nx::vms::api::EventReason::notEnoughIntegrationServices));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, kServerId);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

//-------------------------------------------------------------------------------------------------
// Server Certificate Error

TEST_F(EventLogTest, event_serverCertificateError)
{
    static constexpr auto kExpectedSourceText = "Server1 (localhost)";
    static constexpr auto kExpectedSourceIcon = ResourceType::server;
    static constexpr auto kExpectedDescriptionTooltip = R"(
Server1 (localhost) certificate error
15:46:29
15:48:30
)";

    givenEvent(QSharedPointer<ServerCertificateErrorEvent>::create(
        makeTimestamp("15:46:29"),
        kServerId));

    givenEvent(QSharedPointer<ServerCertificateErrorEvent>::create(
        makeTimestamp("15:48:30"),
        kServerId));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, kServerId);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

//-------------------------------------------------------------------------------------------------
// Server Conflict

TEST_F(EventLogTest, event_serverConflict)
{
    static constexpr auto kExpectedSourceText = "Server1 (localhost)";
    static constexpr auto kExpectedSourceIcon = ResourceType::server;
    static constexpr auto kExpectedDescriptionTooltip = R"(
Server1 (localhost) Conflict
15:46:29
  Discovered a server with the same ID in the same local network:
  Server: 10.0.1.15
15:48:30
  Servers in the same local network have conflict on the following devices:
  Server: 10.0.1.15
  - Entrance (10.0.0.1)
  - a3:57:4a:fd:fa:08
  Server: 10.0.1.16
  - e8:4b:34:1c:6e:8b
  - Camera2 (10.0.0.2)
)";

    // Server1 (localhost) has the same ID as another server on 10.0.1.15. Found using multicast -
    // in the same network.
    givenEvent(QSharedPointer<ServerConflictEvent>::create(
        makeTimestamp("15:46:29"),
        kServerId,
        CameraConflictList{.sourceServer="10.0.1.15"}));

    // Two servers in the same local network but belonging to different sites, use broadcasts to
    // pull video and control the same cameras. Servers required to be merged (or camera control
    // should be disabled on one of them).
    givenEvent(QSharedPointer<ServerConflictEvent>::create(
        makeTimestamp("15:48:30"),
        kServerId,
        CameraConflictList{
            .sourceServer="localhost",
            .camerasByServer={
                {"10.0.1.15", {kCamera1PhysicalId, "a3:57:4a:fd:fa:08"}},
                {"10.0.1.16", {"e8:4b:34:1c:6e:8b", kCamera2PhysicalId}}
            }
        }));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, kServerId);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

//-------------------------------------------------------------------------------------------------
// Server Failure

TEST_F(EventLogTest, event_serverFailure)
{
    static constexpr auto kExpectedSourceText = "Server1 (localhost)";
    static constexpr auto kExpectedSourceIcon = ResourceType::server;
    static constexpr auto kExpectedDescriptionTooltip = R"(
Server1 (localhost) Failure
15:46:29
  Server stopped unexpectedly.
15:48:30
  Connection to server is lost.
)";

    givenEvent(QSharedPointer<ServerFailureEvent>::create(
        makeTimestamp("15:46:29"),
        kServerId,
        nx::vms::api::EventReason::serverStarted));

    givenEvent(QSharedPointer<ServerFailureEvent>::create(
        makeTimestamp("15:48:30"),
        kServerId,
        nx::vms::api::EventReason::serverTerminated));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, kServerId);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

//-------------------------------------------------------------------------------------------------
// Server Started

TEST_F(EventLogTest, event_serverStarted)
{
    static constexpr auto kExpectedSourceText = "Server1 (localhost)";
    static constexpr auto kExpectedSourceIcon = ResourceType::server;
    static constexpr auto kExpectedDescriptionTooltip = R"(
Server1 (localhost) Started
15:46:29
15:48:30
)";

    givenEvent(QSharedPointer<ServerStartedEvent>::create(
        makeTimestamp("15:46:29"),
        kServerId));

    givenEvent(QSharedPointer<ServerStartedEvent>::create(
        makeTimestamp("15:48:30"),
        kServerId));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, kServerId);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

//-------------------------------------------------------------------------------------------------
// Soft Trigger

TEST_F(EventLogTest, event_softTrigger)
{
    static constexpr auto kExpectedSourceText = "Entrance (10.0.0.1)";
    static constexpr auto kExpectedSourceIcon = ResourceType::device;
    static constexpr auto kExpectedDescriptionTooltip = R"(
Soft Trigger Button 1
15:46:29
  Source: Entrance (10.0.0.1)
  User: User1
15:48:30
  Source: Camera2 (10.0.0.2)
  User: User2
)";

    static const Uuid kTriggerId = Uuid::createUuid();
    givenEvent(QSharedPointer<SoftTriggerEvent>::create(
        makeTimestamp("15:46:29"),
        State::instant,
        kTriggerId,
        kCamera1Id,
        kUser1Id,
        "Button 1",
        "some_icon"));

    givenEvent(QSharedPointer<SoftTriggerEvent>::create(
        makeTimestamp("15:48:30"),
        State::instant,
        kTriggerId,
        kCamera2Id,
        kUser2Id,
        "Button 1",
        "some_icon"));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, kCamera1Id);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

//-------------------------------------------------------------------------------------------------
// Storage Issue

TEST_F(EventLogTest, event_storageIssue)
{
    static constexpr auto kExpectedSourceText = "Server1 (localhost)";
    static constexpr auto kExpectedSourceIcon = ResourceType::server;
    static constexpr auto kExpectedDescriptionTooltip = R"(
Storage Issue at Server1 (localhost)
15:46:29
  System disk "C" is almost full.
15:48:30
  I/O error has occurred at disk D.
)";

    givenEvent(QSharedPointer<StorageIssueEvent>::create(
        makeTimestamp("15:46:29"),
        kServerId,
        nx::vms::api::EventReason::systemStorageFull,
        /*reasonText*/ "C"));

    givenEvent(QSharedPointer<StorageIssueEvent>::create(
        makeTimestamp("15:48:30"),
        kServerId,
        nx::vms::api::EventReason::storageIoError,
        /*reasonText*/ "disk D"));

    thenSourceIs(kExpectedSourceText, kExpectedSourceIcon, kServerId);
    thenDescriptionTooltipIs(kExpectedDescriptionTooltip);
}

} // namespace nx::vms::rules::test
