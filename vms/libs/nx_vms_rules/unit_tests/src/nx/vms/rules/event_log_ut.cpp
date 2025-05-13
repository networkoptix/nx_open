// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>

#include <QtCore/QDateTime>
#include <QtNetwork/QHostAddress>

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

static const milliseconds kTimestamp1{QDateTime::currentMSecsSinceEpoch()};
static const milliseconds kTimestamp2 = kTimestamp1 + 10s;

} // namespace

class EventLogTest: public EventDetailsTestBase
{
protected:
    void givenEvent(EventPtr event)
    {
        m_events.push_back(std::move(event));
    }

    void ensureAggregationIsValid()
    {
        NX_ASSERT(!m_events.empty());
        const auto aggregationKey = m_events[0]->aggregationKey();
        for (int i = 1; i < m_events.size(); ++i)
            NX_ASSERT(m_events[i]->aggregationKey() == aggregationKey);
    }

    void thenSourceIs(
        const QString& expectedText,
        std::pair<ResourceType, bool /*plural*/> expectedIcon,
        const UuidList& expectedResourceIds)
    {
        NX_ASSERT(!m_events.empty());
        ensureAggregationIsValid();

        std::vector<EventPtr> eventList;
        std::swap(m_events, eventList);
        const auto event = AggregatedEventPtr::create(std::move(eventList));

        const auto resourceIds = utils::eventSourceResourceIds(event, systemContext());

        EXPECT_EQ(expectedText, utils::eventSourceText(event, systemContext(), Qn::RI_WithUrl));
        EXPECT_EQ(expectedIcon, utils::eventSourceIcon(event, systemContext()));
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

private:
    std::vector<EventPtr> m_events;
};

// FIXME: #sivanov Short plan:
// - Add analytics engines to sources of the analytics events.
// - Support multiple cameras in Generic / Soft Trigger events.
// - Implement aggregation support.
// - List cameras in Saas Issue / Server Conflict / Device IP Conflict / License Issue events.
// - Decide what to do with Generic Events with different sources.
// - Support removed devices.

//-------------------------------------------------------------------------------------------------
// Analytics Event

TEST_F(EventLogTest, event_analytics)
{
    static const nx::Uuid kEngine2Id = nx::Uuid::createUuid();
    static const QString kEventTypeId = "nx.LineCrossing";
    static const nx::Uuid kObjectTrack1Id = nx::Uuid::createUuid();
    static const nx::Uuid kObjectTrack2Id = nx::Uuid::createUuid();
    static const QString kKey1 = "key1";
    static const QString kKey2 = "key2";
    static const QRectF kBoundingBox1(0.1, 0.2, 0.3, 0.4);
    static const QRectF kBoundingBox2(0.5, 0.6, 0.7, 0.8);

    static constexpr auto kExpectedText = "Entrance (10.0.0.1)";
    static constexpr auto kExpectedIcon = ResourceType::device;

    givenEvent(QSharedPointer<AnalyticsEvent>::create(
        kTimestamp1,
        State::started,
        "Caption 1",
        "Description 1",
        kCamera1Id,
        kEngine1Id,
        kEventTypeId,
        nx::common::metadata::Attributes(),
        kObjectTrack1Id,
        kKey1,
        kBoundingBox1
    ));

    givenEvent(QSharedPointer<AnalyticsEvent>::create(
        kTimestamp2,
        State::started,
        "Caption 2",
        "Description 2",
        kCamera1Id,
        kEngine2Id,
        kEventTypeId, //< Event type is fixed in UI.
        nx::common::metadata::Attributes(),
        kObjectTrack2Id,
        kKey2,
        kBoundingBox2
    ));

    thenSourceIs(kExpectedText, kExpectedIcon, kCamera1Id);
}

//-------------------------------------------------------------------------------------------------
// Analytics Object

TEST_F(EventLogTest, event_analyticsObject)
{
    static const nx::Uuid kEngine2Id = nx::Uuid::createUuid();
    static const QString kObjectTypeId1 = "nx.LicensePlate";
    static const QString kObjectTypeId2 = "nx.Face";
    static const nx::Uuid kObjectTrackId1 = nx::Uuid::createUuid();
    static const nx::Uuid kObjectTrackId2 = nx::Uuid::createUuid();

    static constexpr auto kExpectedText = "Entrance (10.0.0.1)";
    static constexpr auto kExpectedIcon = ResourceType::device;

    givenEvent(QSharedPointer<AnalyticsObjectEvent>::create(
        State::started,
        kTimestamp1,
        kCamera1Id,
        kEngine1Id,
        kObjectTypeId1,
        kObjectTrackId1,
        nx::common::metadata::Attributes()
    ));

    givenEvent(QSharedPointer<AnalyticsObjectEvent>::create(
        State::started,
        kTimestamp2,
        kCamera1Id,
        kEngine2Id,
        kObjectTypeId2, //< TODO: sivanov Probably fixed in UI.
        kObjectTrackId2,
        nx::common::metadata::Attributes()
    ));

    thenSourceIs(kExpectedText, kExpectedIcon, kCamera1Id);
}

//-------------------------------------------------------------------------------------------------
// Camera Input

TEST_F(EventLogTest, event_cameraInput)
{
    static constexpr auto kExpectedText = "Entrance (10.0.0.1)";
    static constexpr auto kExpectedIcon = ResourceType::device;

    givenEvent(QSharedPointer<CameraInputEvent>::create(
        kTimestamp1,
        State::started,
        kCamera1Id,
        "Port_1"
    ));

    givenEvent(QSharedPointer<CameraInputEvent>::create(
        kTimestamp2,
        State::started,
        kCamera1Id,
        "Port_2"
    ));

    thenSourceIs(kExpectedText, kExpectedIcon, kCamera1Id);
}

//-------------------------------------------------------------------------------------------------
// Device Disconnected

TEST_F(EventLogTest, event_deviceDisconnected)
{
    static constexpr auto kExpectedText = "Entrance (10.0.0.1)";
    static constexpr auto kExpectedIcon = ResourceType::device;

    givenEvent(QSharedPointer<DeviceDisconnectedEvent>::create(
        kTimestamp1,
        kCamera1Id,
        kServerId
    ));

    givenEvent(QSharedPointer<DeviceDisconnectedEvent>::create(
        kTimestamp2,
        kCamera2Id,
        kServerId
    ));

    thenSourceIs(kExpectedText, kExpectedIcon, kCamera1Id);
}

//-------------------------------------------------------------------------------------------------
// Device IP Confict

TEST_F(EventLogTest, event_deviceIpConflict)
{
    static constexpr auto kExpectedText = "Server1 (localhost)";
    static constexpr auto kExpectedIcon = ResourceType::server;

    givenEvent(QSharedPointer<DeviceIpConflictEvent>::create(
        kTimestamp1,
        kServerId,
        QList<DeviceIpConflictEvent::DeviceInfo>{
            {.id = kCamera1Id, .mac = kCamera1PhysicalId},
            {.id = kCamera2Id, .mac = kCamera2PhysicalId}},
        /*address*/ QHostAddress("10.0.0.1")));

    givenEvent(QSharedPointer<DeviceIpConflictEvent>::create(
        kTimestamp2,
        kServerId,
        QList<DeviceIpConflictEvent::DeviceInfo>{
            {.id = kCamera1Id, .mac = kCamera1PhysicalId},
            {.id = kCamera2Id, .mac = kCamera2PhysicalId}},
        /*address*/ QHostAddress("10.0.0.2")));

    thenSourceIs(kExpectedText, kExpectedIcon, kServerId);
}

//-------------------------------------------------------------------------------------------------
// Fan Error

TEST_F(EventLogTest, event_fanError)
{
    static constexpr auto kExpectedText = "Server1 (localhost)";
    static constexpr auto kExpectedIcon = ResourceType::server;

    givenEvent(QSharedPointer<FanErrorEvent>::create(
        kTimestamp1,
        kServerId));

    givenEvent(QSharedPointer<FanErrorEvent>::create(
        kTimestamp2,
        kServerId));

    thenSourceIs(kExpectedText, kExpectedIcon, kServerId);
}

//-------------------------------------------------------------------------------------------------
// Generic Event

TEST_F(EventLogTest, event_generic_withSourceOnly)
{
    static constexpr auto kExpectedText = "Source 1";
    static constexpr auto kExpectedIcon = ResourceType::server;
    static const Uuid kExpectedResourceId = kServerId;

    givenEvent(QSharedPointer<GenericEvent>::create(
        kTimestamp1,
        State::instant,
        /*caption*/ "Caption 1",
        /*description*/ "Description 1",
        /*source*/ "Source 1",
        kServerId,
        UuidList{}));

    thenSourceIs(kExpectedText, kExpectedIcon, kExpectedResourceId);
}

TEST_F(EventLogTest, event_generic_withSourceAndDevice)
{
    static constexpr auto kExpectedText = "Source 1";
    static constexpr auto kExpectedIcon = ResourceType::device;
    static const Uuid kExpectedResourceId = kCamera1Id;

    givenEvent(QSharedPointer<GenericEvent>::create(
        kTimestamp1,
        State::instant,
        /*caption*/ "Caption 1",
        /*description*/ "Description 1",
        /*source*/ "Source 1",
        kServerId,
        UuidList{kCamera1Id}));

    thenSourceIs(kExpectedText, kExpectedIcon, kExpectedResourceId);
}

TEST_F(EventLogTest, event_generic_withDevicesOnly)
{
    static constexpr auto kExpectedText = "Entrance (10.0.0.1)";
    static constexpr auto kExpectedIcon = ResourceType::device;
    static const Uuid kExpectedResourceId = kCamera1Id;

    givenEvent(QSharedPointer<GenericEvent>::create(
        kTimestamp1,
        State::instant,
        /*caption*/ "Caption 1",
        /*description*/ "Description 1",
        /*source*/ QString(),
        kServerId,
        UuidList{kCamera1Id}));

    thenSourceIs(kExpectedText, kExpectedIcon, kExpectedResourceId);
}

TEST_F(EventLogTest, event_generic_withServerOnly)
{
    static constexpr auto kExpectedText = "Server1 (localhost)";
    static constexpr auto kExpectedIcon = ResourceType::server;
    static const Uuid kExpectedResourceId = kServerId;

    givenEvent(QSharedPointer<GenericEvent>::create(
        kTimestamp1,
        State::instant,
        /*caption*/ "Caption 1",
        /*description*/ "Description 1",
        /*source*/ QString(),
        kServerId,
        UuidList{}));

    thenSourceIs(kExpectedText, kExpectedIcon, kExpectedResourceId);
}

//-------------------------------------------------------------------------------------------------
// LDAP Sync Issue

TEST_F(EventLogTest, event_ldapSyncIssue)
{
    static constexpr auto kExpectedText = "Server1 (localhost)";
    static constexpr auto kExpectedIcon = ResourceType::server;

    givenEvent(QSharedPointer<LdapSyncIssueEvent>::create(
        kTimestamp1,
        nx::vms::api::EventReason::failedToCompleteSyncWithLdap,
        5min,
        kServerId));

    givenEvent(QSharedPointer<LdapSyncIssueEvent>::create(
        kTimestamp2,
        nx::vms::api::EventReason::failedToConnectToLdap,
        30s,
        kServerId));

    thenSourceIs(kExpectedText, kExpectedIcon, kServerId);
}

//-------------------------------------------------------------------------------------------------
// License Issue

TEST_F(EventLogTest, event_licenseIssue)
{
    static constexpr auto kExpectedText = "Server1 (localhost)";
    static constexpr auto kExpectedIcon = ResourceType::server;

    givenEvent(QSharedPointer<LicenseIssueEvent>::create(
        kTimestamp1,
        kServerId,
        /*disabledCameras*/ UuidSet{kCamera1Id}));

    givenEvent(QSharedPointer<LicenseIssueEvent>::create(
        kTimestamp2,
        kServerId,
        /*disabledCameras*/ UuidSet{kCamera1Id, kCamera2Id}));

    thenSourceIs(kExpectedText, kExpectedIcon, kServerId);
}

//-------------------------------------------------------------------------------------------------
// Camera Motion

TEST_F(EventLogTest, event_motion)
{
    static constexpr auto kExpectedText = "Entrance (10.0.0.1)";
    static constexpr auto kExpectedIcon = ResourceType::device;

    givenEvent(QSharedPointer<MotionEvent>::create(
        kTimestamp1,
        State::started,
        kCamera1Id
    ));
    givenEvent(QSharedPointer<MotionEvent>::create(
        kTimestamp2,
        State::started,
        kCamera1Id
    ));

    thenSourceIs(kExpectedText, kExpectedIcon, kCamera1Id);
}

//-------------------------------------------------------------------------------------------------
// Network Issue

TEST_F(EventLogTest, event_networkIssue)
{
    static constexpr auto kExpectedText = "Entrance (10.0.0.1)";
    static constexpr auto kExpectedIcon = ResourceType::device;

    givenEvent(QSharedPointer<NetworkIssueEvent>::create(
        kTimestamp1,
        kCamera1Id,
        kServerId,
        nx::vms::api::EventReason::networkMulticastAddressConflict,
        NetworkIssueInfo{
            .address = "10.0.0.1:5555",
            .deviceName = "testcamera",
            .stream = nx::vms::api::StreamIndex::secondary
        }));

    givenEvent(QSharedPointer<NetworkIssueEvent>::create(
        kTimestamp2,
        kCamera2Id,
        kServerId,
        nx::vms::api::EventReason::networkNoFrame,
        NetworkIssueInfo{
            .timeout = 5s
        }));

    thenSourceIs(kExpectedText, kExpectedIcon, kCamera1Id);
}

//-------------------------------------------------------------------------------------------------
// Integration Diagnostic

TEST_F(EventLogTest, event_integrationDiagnostic_deviceAgent)
{
    static constexpr auto kExpectedText = "Entrance (10.0.0.1)";
    static constexpr auto kExpectedIcon = ResourceType::device;

    givenEvent(QSharedPointer<IntegrationDiagnosticEvent>::create(
        kTimestamp1,
        "Caption 1",
        /*description*/ QString(),
        kCamera1Id,
        kEngine1Id,
        nx::vms::api::EventLevel::error
    ));

    givenEvent(QSharedPointer<IntegrationDiagnosticEvent>::create(
        kTimestamp2,
        /*caption*/ QString(),
        "Description 2",
        kCamera1Id,
        kEngine1Id, //< Engine is fixed in UI.
        nx::vms::api::EventLevel::info
    ));

    thenSourceIs(kExpectedText, kExpectedIcon, kCamera1Id);
}


TEST_F(EventLogTest, event_integrationDiagnostic_plugin)
{
    static constexpr auto kExpectedText = "Engine1";
    static constexpr auto kExpectedIcon = ResourceType::analyticsEngine;
    static const Uuid kExpectedResourceId = kEngine1Id;

    givenEvent(QSharedPointer<IntegrationDiagnosticEvent>::create(
        kTimestamp1,
        "Caption 1",
        /*description*/ QString(),
        /*deviceId*/ nx::Uuid(),
        kEngine1Id,
        nx::vms::api::EventLevel::error
    ));

    givenEvent(QSharedPointer<IntegrationDiagnosticEvent>::create(
        kTimestamp2,
        /*caption*/ QString(),
        "Description 2",
        /*deviceId*/ nx::Uuid(),
        kEngine1Id, //< Engine is fixed in UI.
        nx::vms::api::EventLevel::info
    ));

    thenSourceIs(kExpectedText, kExpectedIcon, kExpectedResourceId);
}

//-------------------------------------------------------------------------------------------------
// POE Over Budget

TEST_F(EventLogTest, event_poeOverBudget)
{
    static constexpr auto kExpectedText = "Server1 (localhost)";
    static constexpr auto kExpectedIcon = ResourceType::server;

    givenEvent(QSharedPointer<PoeOverBudgetEvent>::create(
        kTimestamp1,
        State::instant,
        kServerId,
        /*currentConsumptionW*/ 0.52,
        /*upperLimitW*/ 0.35,
        /*lowerLimitW*/ 0.18));

    givenEvent(QSharedPointer<PoeOverBudgetEvent>::create(
        kTimestamp2,
        State::instant,
        kServerId,
        /*currentConsumptionW*/ 0.85,
        /*upperLimitW*/ 0.2,
        /*lowerLimitW*/ 0.1));

    thenSourceIs(kExpectedText, kExpectedIcon, kServerId);
}

//-------------------------------------------------------------------------------------------------
// SaaS Issue

TEST_F(EventLogTest, event_saasIssue)
{
    static constexpr auto kExpectedText = "Server1 (localhost)";
    static constexpr auto kExpectedIcon = ResourceType::server;

    givenEvent(QSharedPointer<SaasIssueEvent>::create(
        kTimestamp1,
        kServerId,
        /*licenseKeys*/ QStringList{"key1", "key2"},
        nx::vms::api::EventReason::licenseMigrationFailed));

    givenEvent(QSharedPointer<SaasIssueEvent>::create(
        kTimestamp2,
        kServerId,
        /*deviceIds*/ UuidList{kCamera1Id, kCamera2Id},
        nx::vms::api::EventReason::notEnoughIntegrationServices));

    thenSourceIs(kExpectedText, kExpectedIcon, kServerId);
}

//-------------------------------------------------------------------------------------------------
// Server Certificate Error

TEST_F(EventLogTest, event_serverCertificateError)
{
    static constexpr auto kExpectedText = "Server1 (localhost)";
    static constexpr auto kExpectedIcon = ResourceType::server;

    givenEvent(QSharedPointer<ServerCertificateErrorEvent>::create(
        kTimestamp1,
        kServerId));

    givenEvent(QSharedPointer<ServerCertificateErrorEvent>::create(
        kTimestamp2,
        kServerId));

    thenSourceIs(kExpectedText, kExpectedIcon, kServerId);
}

//-------------------------------------------------------------------------------------------------
// Server Conflict

TEST_F(EventLogTest, event_serverConflict)
{
    static constexpr auto kExpectedText = "Server1 (localhost)";
    static constexpr auto kExpectedIcon = ResourceType::server;

    // Server1 (localhost) has the same ID as another server on 10.0.1.15. Found using multicast -
    // in the same network.
    givenEvent(QSharedPointer<ServerConflictEvent>::create(
        kTimestamp1,
        kServerId,
        CameraConflictList{.sourceServer="10.0.1.15"}));

    // Two servers in the same local network but belonging to different sites, use broadcasts to
    // pull video and control the same cameras. Servers required to be merged (or camera control
    // should be disabled on one of them).
    givenEvent(QSharedPointer<ServerConflictEvent>::create(
        kTimestamp2,
        kServerId,
        CameraConflictList{
            .sourceServer="localhost",
            .camerasByServer={
                {"10.0.1.15", {kCamera1PhysicalId, "a3:57:4a:fd:fa:08"}},
                {"10.0.1.16", {"e8:4b:34:1c:6e:8b", kCamera2PhysicalId}}
            }
        }));

    thenSourceIs(kExpectedText, kExpectedIcon, kServerId);
}

//-------------------------------------------------------------------------------------------------
// Server Failure

TEST_F(EventLogTest, event_serverFailure)
{
    static constexpr auto kExpectedText = "Server1 (localhost)";
    static constexpr auto kExpectedIcon = ResourceType::server;

    givenEvent(QSharedPointer<ServerFailureEvent>::create(
        kTimestamp1,
        kServerId,
        nx::vms::api::EventReason::serverStarted));

    givenEvent(QSharedPointer<ServerFailureEvent>::create(
        kTimestamp2,
        kServerId,
        nx::vms::api::EventReason::serverTerminated));

    thenSourceIs(kExpectedText, kExpectedIcon, kServerId);
}

//-------------------------------------------------------------------------------------------------
// Server Started

TEST_F(EventLogTest, event_serverStarted)
{
    static constexpr auto kExpectedText = "Server1 (localhost)";
    static constexpr auto kExpectedIcon = ResourceType::server;

    givenEvent(QSharedPointer<ServerStartedEvent>::create(
        kTimestamp1,
        kServerId));

    givenEvent(QSharedPointer<ServerStartedEvent>::create(
        kTimestamp2,
        kServerId));

    thenSourceIs(kExpectedText, kExpectedIcon, kServerId);
}

//-------------------------------------------------------------------------------------------------
// Soft Trigger

TEST_F(EventLogTest, event_softTrigger)
{
    static constexpr auto kExpectedText = "Entrance (10.0.0.1)";
    static constexpr auto kExpectedIcon = ResourceType::device;

    static const Uuid kTriggerId = Uuid::createUuid();
    givenEvent(QSharedPointer<SoftTriggerEvent>::create(
        kTimestamp1,
        State::instant,
        kTriggerId,
        kCamera1Id,
        kUser1Id,
        "Button 1",
        "some_icon"));

    givenEvent(QSharedPointer<SoftTriggerEvent>::create(
        kTimestamp2,
        State::instant,
        kTriggerId,
        kCamera2Id,
        kUser2Id,
        "Button 1",
        "some_icon"));

    thenSourceIs(kExpectedText, kExpectedIcon, kCamera1Id);
}

//-------------------------------------------------------------------------------------------------
// Storage Issue

TEST_F(EventLogTest, event_storageIssue)
{
    static constexpr auto kExpectedText = "Server1 (localhost)";
    static constexpr auto kExpectedIcon = ResourceType::server;

    givenEvent(QSharedPointer<StorageIssueEvent>::create(
        kTimestamp1,
        kServerId,
        nx::vms::api::EventReason::systemStorageFull,
        /*reasonText*/ "C"));

    givenEvent(QSharedPointer<StorageIssueEvent>::create(
        kTimestamp2,
        kServerId,
        nx::vms::api::EventReason::storageIoError,
        /*reasonText*/ "disk D"));

    thenSourceIs(kExpectedText, kExpectedIcon, kServerId);
}

} // namespace nx::vms::rules::test
