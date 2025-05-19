// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <ranges>

#include <QtNetwork/QHostAddress>

#include <nx/vms/rules/action_builder_fields/text_with_fields.h>
#include <nx/vms/rules/actions/push_notification_action.h>
#include <nx/vms/rules/actions/show_notification_action.h>
#include <nx/vms/rules/aggregated_event.h>
#include <nx/vms/rules/events/builtin_events.h>
#include <nx/vms/rules/manifest.h>
#include <nx/vms/rules/utils/field_names.h>

#include "event_details_test_base.h"

using namespace std::chrono;

namespace nx::vms::rules::test {

namespace {

const FieldDescriptor kTestCaptionActionFieldDescriptor{.type = "testCaption"};
const FieldDescriptor kTestDescriptionActionFieldDescriptor{.type = "testDescription"};
const FieldDescriptor kTestTooltipActionFieldDescriptor{.type = "testTooltip"};

/** Return timestamp from time like: "15:46:29" with an example date. */
std::chrono::milliseconds makeTimestamp(const QString& time)
{
    static const QString kTimeFormat = "hh:mm:ss";
    static const QDate kDate(2025, 10, 25); //< Date is not displayed in notifications.

    const QDateTime dt(kDate, QTime::fromString(time, kTimeFormat));
    return std::chrono::milliseconds(dt.toMSecsSinceEpoch());
}

} // namespace

class NotificationActionsTest: public EventDetailsTestBase
{
protected:
    virtual void SetUp() override
    {
        m_captionField = std::make_unique<TextWithFields>(
            systemContext(),
            &kTestCaptionActionFieldDescriptor);
        m_captionField->setText("{event.caption}");

        m_descriptionField = std::make_unique<TextWithFields>(
            systemContext(),
            &kTestDescriptionActionFieldDescriptor);
        m_descriptionField->setText("{event.description}");

        m_tooltipField = std::make_unique<TextWithFields>(
            systemContext(),
            &kTestTooltipActionFieldDescriptor);
        m_tooltipField->setText("{event.extendedDescription}");
    }

    void whenEventsLimitSetTo(int value)
    {
        m_tooltipField->setSubstitutionEventsLimit(value);
    }

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

    void thenNotificationIs(
        const QString& expectedCaption,
        const QString& expectedDescription,
        const QString& expectedTooltip)
    {
        NX_ASSERT(!m_events.empty());
        ensureAggregationIsValid();

        std::vector<EventPtr> eventList;
        std::swap(m_events, eventList);

        const auto event = AggregatedEventPtr::create(std::move(eventList));

        const auto caption = m_captionField->build(event).value<QString>();
        const auto description = m_descriptionField->build(event).value<QString>();
        const auto tooltip = m_tooltipField->build(event).value<QString>();

        EXPECT_EQ(expectedCaption, caption);
        EXPECT_EQ(expectedDescription.trimmed(), description);
        EXPECT_EQ(expectedTooltip.trimmed(), tooltip);
    }

protected:
    std::unique_ptr<TextWithFields> m_captionField;
    std::unique_ptr<TextWithFields> m_descriptionField;
    std::unique_ptr<TextWithFields> m_tooltipField;
    std::vector<EventPtr> m_events;
};

// FIXME: #sivanov Short plan:
// - Details cache to avoid repopulating.
// - Validate screenshots.
// - Add tests on a taxonomy.

//-------------------------------------------------------------------------------------------------
// Content tests

TEST_F(NotificationActionsTest, content_notificationAction)
{
    const auto manifest = NotificationAction::manifest();

    {
        auto captionField = std::ranges::find_if(manifest.fields,
            [](const auto& f) { return f.fieldName == utils::kCaptionFieldName; });
        ASSERT_NE(captionField, manifest.fields.end());
        ASSERT_EQ(captionField->properties["text"].toString(), m_captionField->text());
    }

    {
        auto descriptionField = std::ranges::find_if(manifest.fields,
            [](const auto& f) { return f.fieldName == utils::kDescriptionFieldName; });
        ASSERT_NE(descriptionField, manifest.fields.end());
        ASSERT_EQ(descriptionField->properties["text"].toString(), m_descriptionField->text());
    }

    {
        auto tooltipField = std::ranges::find_if(manifest.fields,
            [](const auto& f) { return f.fieldName == utils::kTooltipFieldName; });
        ASSERT_NE(tooltipField, manifest.fields.end());
        ASSERT_EQ(tooltipField->properties["text"].toString(), m_tooltipField->text());
    }
}

TEST_F(NotificationActionsTest, content_pushNotificationAction)
{
    const auto manifest = PushNotificationAction::manifest();

    {
        auto captionField = std::ranges::find_if(manifest.fields,
            [](const auto& f) { return f.fieldName == utils::kCaptionFieldName; });
        ASSERT_NE(captionField, manifest.fields.end());
        ASSERT_EQ(captionField->properties["text"].toString(), m_captionField->text());
    }

    {
        auto descriptionField = std::ranges::find_if(manifest.fields,
            [](const auto& f) { return f.fieldName == utils::kDescriptionFieldName; });
        ASSERT_NE(descriptionField, manifest.fields.end());
        ASSERT_EQ(descriptionField->properties["text"].toString(), m_descriptionField->text());
    }
}

TEST_F(NotificationActionsTest, content_eventsPerDayLimit)
{
    static constexpr auto kExpectedCaption = "Server Started";

    static constexpr auto kExpectedTooltip = R"(
Server1 Started
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
    thenNotificationIs(kExpectedCaption, {}, kExpectedTooltip);
}

//-------------------------------------------------------------------------------------------------
// Analytics Event

TEST_F(NotificationActionsTest, event_analytics)
{
    static const QString kEventTypeId = "nx.LineCrossing";

    static constexpr auto kExpectedCaption = "Caption 1";
    static constexpr auto kExpectedDescription = "Description 1";

    static constexpr auto kExpectedTooltip = R"(
Analytics Event at Entrance
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
        /*engineId*/ nx::Uuid::createUuid(),
        kEventTypeId, //< Event type is fixed in UI.
        nx::common::metadata::Attributes(),
        /*objectTrackId*/ nx::Uuid::createUuid(),
        /*key*/ QString{},
        /*boundingBox*/ QRectF{}
    ));

    thenNotificationIs(kExpectedCaption, kExpectedDescription, kExpectedTooltip);
}

TEST_F(NotificationActionsTest, event_analytics_descriptionOnly)
{
    static const QString kEventTypeId = "nx.LineCrossing";

    static constexpr auto kExpectedCaption = "Analytics Event";
    static constexpr auto kExpectedDescription = "Description 1";

    static constexpr auto kExpectedTooltip = R"(
Analytics Event at Entrance
15:46:29
  Description: Description 1
)";

    givenEvent(QSharedPointer<AnalyticsEvent>::create(
        makeTimestamp("15:46:29"),
        State::started,
        /*caption*/ QString(),
        "Description 1",
        kCamera1Id,
        kEngine1Id,
        kEventTypeId,
        nx::common::metadata::Attributes(),
        /*objectTrackId*/ nx::Uuid::createUuid(),
        /*key*/ QString{},
        /*boundingBox*/ QRectF{}
    ));

    thenNotificationIs(kExpectedCaption, kExpectedDescription, kExpectedTooltip);
}

TEST_F(NotificationActionsTest, event_analytics_captionOnly)
{
    static const QString kEventTypeId = "nx.LineCrossing";

    static constexpr auto kExpectedCaption = "Caption 1";

    static constexpr auto kExpectedTooltip = R"(
Analytics Event at Entrance
15:46:29
  Caption: Caption 1
)";

    givenEvent(QSharedPointer<AnalyticsEvent>::create(
        makeTimestamp("15:46:29"),
        State::started,
        "Caption 1",
        /*description*/ QString(),
        kCamera1Id,
        kEngine1Id,
        kEventTypeId,
        nx::common::metadata::Attributes(),
        /*objectTrackId*/ nx::Uuid::createUuid(),
        /*key*/ QString{},
        /*boundingBox*/ QRectF{}
    ));

    thenNotificationIs(kExpectedCaption, {}, kExpectedTooltip);
}

//-------------------------------------------------------------------------------------------------
// Analytics Object

TEST_F(NotificationActionsTest, event_analyticsObject)
{
    static const nx::Uuid kEngine2Id = nx::Uuid::createUuid();
    static const QString kObjectTypeId1 = "nx.LicensePlate";
    static const QString kObjectTypeId2 = "nx.Face";
    static const nx::Uuid kObjectTrackId1 = nx::Uuid::createUuid();
    static const nx::Uuid kObjectTrackId2 = nx::Uuid::createUuid();

    static constexpr auto kExpectedCaption = "Object detected";

    static constexpr auto kExpectedTooltip = R"(
Object detected at Entrance
15:46:29
15:48:30
)";

    givenEvent(QSharedPointer<AnalyticsObjectEvent>::create(
        State::started,
        makeTimestamp("15:46:29"),
        kCamera1Id,
        kEngine1Id,
        kObjectTypeId1,
        kObjectTrackId1,
        nx::common::metadata::Attributes()
    ));

    givenEvent(QSharedPointer<AnalyticsObjectEvent>::create(
        State::started,
        makeTimestamp("15:48:30"),
        kCamera1Id,
        kEngine2Id,
        kObjectTypeId2, //< TODO: sivanov Probably fixed in UI.
        kObjectTrackId2,
        nx::common::metadata::Attributes()
    ));

    thenNotificationIs(kExpectedCaption, {}, kExpectedTooltip);
}

//-------------------------------------------------------------------------------------------------
// Camera Input

TEST_F(NotificationActionsTest, event_cameraInput)
{
    static constexpr auto kExpectedCaption = "Input Signal on Camera";
    static constexpr auto kExpectedDescription = "Input Port: Port_1";

    static constexpr auto kExpectedTooltip = R"(
Input on Entrance
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

    thenNotificationIs(kExpectedCaption, kExpectedDescription, kExpectedTooltip);
}

//-------------------------------------------------------------------------------------------------
// Device Disconnected

TEST_F(NotificationActionsTest, event_deviceDisconnected)
{
    static constexpr auto kExpectedCaption = "Camera disconnected";

    static constexpr auto kExpectedDescription = "Entrance";

    static constexpr auto kExpectedTooltip = R"(
Camera disconnected at Server1
15:46:29
  Entrance
15:48:30
  Camera2
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

    thenNotificationIs(kExpectedCaption, kExpectedDescription, kExpectedTooltip);
}

//-------------------------------------------------------------------------------------------------
// Device IP Conflict

TEST_F(NotificationActionsTest, event_deviceIpConflict)
{
    static constexpr auto kExpectedCaption = "Camera IP Conflict";

    static constexpr auto kExpectedDescription = R"(
Conflicting Address: 10.0.0.1
Camera #1: Entrance (2a:cf:c7:04:c7:c9)
Camera #2: Camera2 (51:dc:54:02:3a:8e)
)";

    static constexpr auto kExpectedTooltip = R"(
Camera IP Conflict at Server1
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

    thenNotificationIs(kExpectedCaption, kExpectedDescription, kExpectedTooltip);
}

//-------------------------------------------------------------------------------------------------
// Fan Error

TEST_F(NotificationActionsTest, event_fanError)
{
    static constexpr auto kExpectedCaption = "Fan Failure";

    static constexpr auto kExpectedTooltip = R"(
Fan failure at Server1
15:46:29
15:48:30
)";

    givenEvent(QSharedPointer<FanErrorEvent>::create(
        makeTimestamp("15:46:29"),
        kServerId));

    givenEvent(QSharedPointer<FanErrorEvent>::create(
        makeTimestamp("15:48:30"),
        kServerId));

    thenNotificationIs(kExpectedCaption, {}, kExpectedTooltip);
}

//-------------------------------------------------------------------------------------------------
// Generic Event

TEST_F(NotificationActionsTest, event_generic)
{
    static constexpr auto kExpectedCaption = "Caption 1";
    static constexpr auto kExpectedDescription = "Description 1";

    static constexpr auto kExpectedTooltip = R"(
Generic Event at Server1
15:46:29
  Source: Source 1
  Caption: Caption 1
  Description: Description 1
  Related cameras:
  - Entrance (10.0.0.1)
  - Camera2 (10.0.0.2)
15:48:30
  Caption: Caption 2
  Description: Description 2
15:48:35
  Source: Source 3
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
        UuidList{kCamera1Id, kCamera2Id}));

    givenEvent(QSharedPointer<GenericEvent>::create(
        makeTimestamp("15:48:30"),
        State::instant,
        /*caption*/ "Caption 2",
        /*description*/ "Description 2",
        /*source*/ QString(),
        kServerId,
        UuidList{}));

    givenEvent(QSharedPointer<GenericEvent>::create(
        makeTimestamp("15:48:35"),
        State::instant,
        /*caption*/ QString(),
        /*description*/ QString(),
        /*source*/ "Source 3",
        kServerId,
        UuidList{kCamera1Id}));

    thenNotificationIs(kExpectedCaption, kExpectedDescription, kExpectedTooltip);
}

TEST_F(NotificationActionsTest, event_generic_captionOnly)
{
    static constexpr auto kExpectedCaption = "Caption 1";

    static constexpr auto kExpectedTooltip = R"(
Generic Event at Server1
15:46:29
  Source: Source 1
  Caption: Caption 1
  Related cameras:
  - Entrance (10.0.0.1)
  - Camera2 (10.0.0.2)
)";

    givenEvent(QSharedPointer<GenericEvent>::create(
        makeTimestamp("15:46:29"),
        State::instant,
        /*caption*/ "Caption 1",
        /*description*/ QString(),
        /*source*/ "Source 1",
        kServerId,
        UuidList{kCamera1Id, kCamera2Id}));

    thenNotificationIs(kExpectedCaption, {}, kExpectedTooltip);
}

TEST_F(NotificationActionsTest, event_generic_descriptionOnly)
{
    static constexpr auto kExpectedCaption = "Generic Event";
    static constexpr auto kExpectedDescription = "Description 1";

    static constexpr auto kExpectedTooltip = R"(
Generic Event at Server1
15:46:29
  Source: Source 1
  Description: Description 1
  Related cameras:
  - Entrance (10.0.0.1)
  - Camera2 (10.0.0.2)
)";

    givenEvent(QSharedPointer<GenericEvent>::create(
        makeTimestamp("15:46:29"),
        State::instant,
        /*caption*/ QString(),
        /*description*/ "Description 1",
        /*source*/ "Source 1",
        kServerId,
        UuidList{kCamera1Id, kCamera2Id}));

    thenNotificationIs(kExpectedCaption, kExpectedDescription, kExpectedTooltip);
}

//-------------------------------------------------------------------------------------------------
// LDAP Sync Issue

TEST_F(NotificationActionsTest, event_ldapSyncIssue)
{
    static constexpr auto kExpectedCaption = "LDAP Sync Issue";

    static constexpr auto kExpectedDescription =
        "Failed to complete the sync within a 5 minutes timeout.";

    static constexpr auto kExpectedTooltip = R"(
LDAP Sync Issue at Server1
15:46:29
  Failed to complete the sync within a 5 minutes timeout.
15:48:30
  Failed to complete the sync within a 3 minutes timeout.
15:48:35
  Failed to connect to the LDAP server.
15:48:45
  No user accounts on LDAP server match the synchronization settings.
15:48:55
  Some LDAP users or groups were not found in the LDAP database.
)";

    givenEvent(QSharedPointer<LdapSyncIssueEvent>::create(
        makeTimestamp("15:46:29"),
        nx::vms::api::EventReason::failedToCompleteSyncWithLdap,
        5min,
        kServerId));

    givenEvent(QSharedPointer<LdapSyncIssueEvent>::create(
        makeTimestamp("15:48:30"),
        nx::vms::api::EventReason::failedToCompleteSyncWithLdap,
        3min,
        kServerId));

    givenEvent(QSharedPointer<LdapSyncIssueEvent>::create(
        makeTimestamp("15:48:35"),
        nx::vms::api::EventReason::failedToConnectToLdap,
        30s,
        kServerId));

    givenEvent(QSharedPointer<LdapSyncIssueEvent>::create(
        makeTimestamp("15:48:45"),
        nx::vms::api::EventReason::noLdapUsersAfterSync,
        30s,
        kServerId));

    givenEvent(QSharedPointer<LdapSyncIssueEvent>::create(
        makeTimestamp("15:48:55"),
        nx::vms::api::EventReason::someUsersNotFoundInLdap,
        30s,
        kServerId));

    thenNotificationIs(kExpectedCaption, kExpectedDescription, kExpectedTooltip);
}

//-------------------------------------------------------------------------------------------------
// License Issue

TEST_F(NotificationActionsTest, event_licenseIssue)
{
    static constexpr auto kExpectedCaption = "License Issue";

    static constexpr auto kExpectedDescription = R"(
Recording has been disabled on the following cameras:
- Entrance (10.0.0.1)
)";

    static constexpr auto kExpectedTooltip = R"(
Not enough licenses on Server1
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

    thenNotificationIs(kExpectedCaption, kExpectedDescription, kExpectedTooltip);
}

//-------------------------------------------------------------------------------------------------
// Camera Motion

TEST_F(NotificationActionsTest, event_motion)
{
    static constexpr auto kExpectedCaption = "Motion on Camera";

    static constexpr auto kExpectedTooltip = R"(
Motion on Entrance
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

    thenNotificationIs(kExpectedCaption, {}, kExpectedTooltip);
}

//-------------------------------------------------------------------------------------------------
// Network Issue

TEST_F(NotificationActionsTest, event_networkIssue)
{
    static constexpr auto kExpectedCaption = "Network Issue";

    static constexpr auto kExpectedDescription = R"(
Entrance
Multicast address conflict detected. Address 10.0.0.1:5555 is already in use by testcamera on secondary stream.
)";

    static constexpr auto kExpectedTooltip = R"(
Network Issue at Server1
15:46:29
  Entrance
  Multicast address conflict detected. Address 10.0.0.1:5555 is already in use by testcamera on secondary stream.
15:48:30
  Camera2
  Multicast address conflict detected. Address localhost:1234 is already in use by testcamera 2 on primary stream.
15:50:29
  Camera2
  No data received during last 5 seconds.
15:50:35
  Entrance
  RTP error in secondary stream (Some more details).
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
        nx::vms::api::EventReason::networkMulticastAddressConflict,
        NetworkIssueInfo{
            .address = "localhost:1234",
            .deviceName = "testcamera 2",
            .stream = nx::vms::api::StreamIndex::primary
        }));

    givenEvent(QSharedPointer<NetworkIssueEvent>::create(
        makeTimestamp("15:50:29"),
        kCamera2Id,
        kServerId,
        nx::vms::api::EventReason::networkNoFrame,
        NetworkIssueInfo{
            .timeout = 5s
        }));

    givenEvent(QSharedPointer<NetworkIssueEvent>::create(
        makeTimestamp("15:50:35"),
        kCamera1Id,
        kServerId,
        nx::vms::api::EventReason::networkRtpStreamError,
        NetworkIssueInfo{
            .stream = nx::vms::api::StreamIndex::secondary,
            .timeout = 5s,
            .message = "Some more details"
        }));

    thenNotificationIs(kExpectedCaption, kExpectedDescription, kExpectedTooltip);
}

//-------------------------------------------------------------------------------------------------
// Integration Diagnostic

TEST_F(NotificationActionsTest, event_integrationDiagnostic_deviceAgent)
{
    static constexpr auto kExpectedCaption = "Caption 1";
    static constexpr auto kExpectedDescription = "Description 1";

    static constexpr auto kExpectedTooltip = R"(
Integration Diagnostic at Entrance
15:46:29
  Caption: Caption 1
  Description: Description 1
15:48:30
  Caption: Caption 2
  Description: Description 2
)";

    givenEvent(QSharedPointer<IntegrationDiagnosticEvent>::create(
        makeTimestamp("15:46:29"),
        "Caption 1",
        "Description 1",
        kCamera1Id,
        kEngine1Id,
        nx::vms::api::EventLevel::error
    ));

    givenEvent(QSharedPointer<IntegrationDiagnosticEvent>::create(
        makeTimestamp("15:48:30"),
        "Caption 2",
        "Description 2",
        kCamera1Id,
        kEngine1Id, //< Engine is fixed in UI.
        nx::vms::api::EventLevel::info
    ));

    thenNotificationIs(kExpectedCaption, kExpectedDescription, kExpectedTooltip);
}


TEST_F(NotificationActionsTest, event_integrationDiagnostic_deviceAgent_captionOnly)
{
    static constexpr auto kExpectedCaption = "Caption 1";
    static constexpr auto kExpectedTooltip = R"(
Integration Diagnostic at Entrance
15:46:29
  Caption: Caption 1
)";

    givenEvent(QSharedPointer<IntegrationDiagnosticEvent>::create(
        makeTimestamp("15:46:29"),
        "Caption 1",
        /*description*/ QString(),
        kCamera1Id,
        kEngine1Id,
        nx::vms::api::EventLevel::error
    ));
    thenNotificationIs(kExpectedCaption, {}, kExpectedTooltip);
}

TEST_F(NotificationActionsTest, event_integrationDiagnostic_deviceAgent_descriptionOnly)
{
    static constexpr auto kExpectedCaption = "Integration Diagnostic";
    static constexpr auto kExpectedDescription = "Description 1";
    static constexpr auto kExpectedTooltip = R"(
Integration Diagnostic at Entrance
15:46:29
  Description: Description 1
)";

    givenEvent(QSharedPointer<IntegrationDiagnosticEvent>::create(
        makeTimestamp("15:46:29"),
        /*caption*/ QString(),
        "Description 1",
        kCamera1Id,
        kEngine1Id,
        nx::vms::api::EventLevel::error
    ));

    thenNotificationIs(kExpectedCaption, kExpectedDescription, kExpectedTooltip);
}

TEST_F(NotificationActionsTest, event_integrationDiagnostic_plugin)
{
    static constexpr auto kExpectedCaption = "Caption 1";
    static constexpr auto kExpectedDescription = "Description 1";
    static constexpr auto kExpectedTooltip = R"(
Integration Diagnostic at Engine1
15:46:29
  Caption: Caption 1
  Description: Description 1
15:48:30
  Caption: Caption 2
  Description: Description 2
)";

    givenEvent(QSharedPointer<IntegrationDiagnosticEvent>::create(
        makeTimestamp("15:46:29"),
        "Caption 1",
        "Description 1",
        /*deviceId*/ nx::Uuid(),
        kEngine1Id,
        nx::vms::api::EventLevel::error
    ));

    givenEvent(QSharedPointer<IntegrationDiagnosticEvent>::create(
        makeTimestamp("15:48:30"),
        "Caption 2",
        "Description 2",
        /*deviceId*/ nx::Uuid(),
        kEngine1Id, //< Engine is fixed in UI.
        nx::vms::api::EventLevel::info
    ));

    thenNotificationIs(kExpectedCaption, kExpectedDescription, kExpectedTooltip);
}

//-------------------------------------------------------------------------------------------------
// POE Over Budget

TEST_F(NotificationActionsTest, event_poeOverBudget)
{
    static constexpr auto kExpectedCaption = "PoE Over Budget";

    static constexpr auto kExpectedDescription = R"(
Current power consumption: 0.5 watts
Upper consumption limit: 0.3 watts
Lower consumption limit: 0.2 watts
)";

    static constexpr auto kExpectedTooltip = R"(
PoE over budget on Server1
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

    thenNotificationIs(kExpectedCaption, kExpectedDescription, kExpectedTooltip);
}

//-------------------------------------------------------------------------------------------------
// SaaS Issue

TEST_F(NotificationActionsTest, event_saasIssue)
{
    static constexpr auto kExpectedCaption = "Services Issue";

    static constexpr auto kExpectedDescription = R"(
Failed to migrate licenses.
- key1
- key2
)";

    static constexpr auto kExpectedTooltip = R"(
Services Issue on Server1
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

    thenNotificationIs(kExpectedCaption, kExpectedDescription, kExpectedTooltip);
}

//-------------------------------------------------------------------------------------------------
// Server Certificate Error

TEST_F(NotificationActionsTest, event_serverCertificateError)
{
    static constexpr auto kExpectedCaption = "Server Certificate Error";

    static constexpr auto kExpectedTooltip = R"(
Server1 certificate error
15:46:29
15:48:30
)";

    givenEvent(QSharedPointer<ServerCertificateErrorEvent>::create(
        makeTimestamp("15:46:29"),
        kServerId));

    givenEvent(QSharedPointer<ServerCertificateErrorEvent>::create(
        makeTimestamp("15:48:30"),
        kServerId));

    thenNotificationIs(kExpectedCaption, {}, kExpectedTooltip);
}

//-------------------------------------------------------------------------------------------------
// Server Conflict

TEST_F(NotificationActionsTest, event_serverConflict)
{
    static constexpr auto kExpectedCaption = "Server Conflict";

    static constexpr auto kExpectedDescription = R"(
Discovered a server with the same ID in the same local network:
Server: 10.0.1.15
)";

    static constexpr auto kExpectedTooltip = R"(
Server1 Conflict
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

    thenNotificationIs(kExpectedCaption, kExpectedDescription, kExpectedTooltip);
}

//-------------------------------------------------------------------------------------------------
// Server Failure

TEST_F(NotificationActionsTest, event_serverFailure)
{
    static constexpr auto kExpectedCaption = "Server Failure";

    static constexpr auto kExpectedDescription = R"(
Server stopped unexpectedly.
)";

    static constexpr auto kExpectedTooltip = R"(
Server1 Failure
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

    thenNotificationIs(kExpectedCaption, kExpectedDescription, kExpectedTooltip);
}

//-------------------------------------------------------------------------------------------------
// Server Started

TEST_F(NotificationActionsTest, event_serverStarted)
{
    static constexpr auto kExpectedCaption = "Server Started";

    static constexpr auto kExpectedTooltip = R"(
Server1 Started
15:46:29
15:48:30
)";

    givenEvent(QSharedPointer<ServerStartedEvent>::create(
        makeTimestamp("15:46:29"),
        kServerId));

    givenEvent(QSharedPointer<ServerStartedEvent>::create(
        makeTimestamp("15:48:30"),
        kServerId));

    thenNotificationIs(kExpectedCaption, {}, kExpectedTooltip);
}

//-------------------------------------------------------------------------------------------------
// Soft Trigger

/**
 * Single email is sent for any number of soft trigger happenings for a single trigger instance.
 * There is no camera-based aggregation.
 */
TEST_F(NotificationActionsTest, event_softTrigger)
{
    static constexpr auto kExpectedCaption = "Button 1";

    static constexpr auto kExpectedDescription = R"(
Source: Entrance
User: User1
)";

    static constexpr auto kExpectedTooltip = R"(
Soft Trigger Button 1
15:46:29
  Source: Entrance
  User: User1
15:48:30
  Source: Camera2
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

    thenNotificationIs(kExpectedCaption, kExpectedDescription, kExpectedTooltip);
}

//-------------------------------------------------------------------------------------------------
// Storage Issue

TEST_F(NotificationActionsTest, event_storageIssue)
{
    static constexpr auto kExpectedCaption = "Storage Issue";

    static constexpr auto kExpectedDescription = R"(
System disk "C" is almost full.
)";

    static constexpr auto kExpectedTooltip = R"(
Storage Issue at Server1
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

    thenNotificationIs(kExpectedCaption, kExpectedDescription, kExpectedTooltip);
}

} // namespace nx::vms::rules::test
