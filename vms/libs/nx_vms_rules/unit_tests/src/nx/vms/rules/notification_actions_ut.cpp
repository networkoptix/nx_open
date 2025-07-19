// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <ranges>

#include <QtNetwork/QHostAddress>

#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/rules/action_builder_fields/text_with_fields.h>
#include <nx/vms/rules/actions/push_notification_action.h>
#include <nx/vms/rules/actions/show_notification_action.h>
#include <nx/vms/rules/aggregated_event.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/events/builtin_events.h>
#include <nx/vms/rules/manifest.h>
#include <nx/vms/rules/strings.h>
#include <nx/vms/rules/utils/event_log.h>
#include <nx/vms/rules/utils/field_names.h>

#include "event_details_test_base.h"

using namespace std::chrono;

namespace nx::vms::rules::test {

namespace {

const FieldDescriptor kTestCaptionActionFieldDescriptor{.type = "testCaption"};
const FieldDescriptor kTestDescriptionActionFieldDescriptor{.type = "testDescription"};
const FieldDescriptor kTestTooltipActionFieldDescriptor{.type = "testTooltip"};
constexpr auto kSiteName = "SiteName";

/** Return timestamp from time like: "15:46:29" with an example date. */
std::chrono::milliseconds makeTimestamp(const QString& time)
{
    static const QString kTimeFormat = "hh:mm:ss";
    static const QDate kDate(2025, 10, 25); //< Date is not displayed in notifications.

    const QDateTime dt(kDate, QTime::fromString(time, kTimeFormat));
    return std::chrono::milliseconds(dt.toMSecsSinceEpoch());
}

QString sourceTag(const QString& source)
{
    return QString{"[%1: %2]"}.arg(kSiteName).arg(source);
}

QString descriptionWithSourceTag(const QString& description, const QString& source)
{
    return QString{"%1\n%2"}.arg(description).arg(sourceTag(source));
}

} // namespace


// Desktop and mobile notification contents are checked in the given tests.
class NotificationActionsTest: public EventDetailsTestBase
{
protected:
    virtual void SetUp() override
    {
        const auto settings = systemContext()->globalSettings();
        settings->setSystemName(kSiteName);

        m_captionField = std::make_unique<TextWithFields>(
            systemContext(),
            &kTestCaptionActionFieldDescriptor);
        m_captionField->setText("{event.caption}");

        m_descriptionField = std::make_unique<TextWithFields>(
            systemContext(),
            &kTestDescriptionActionFieldDescriptor);
        m_descriptionField->setText("{event.description}");

        m_pushDescriptionField = std::make_unique<TextWithFields>(
            systemContext(),
            &kTestDescriptionActionFieldDescriptor);
        m_pushDescriptionField->setText("{event.description}\n[{site.name}: {event.source}]");

        m_tooltipField = std::make_unique<TextWithFields>(
            systemContext(),
            &kTestTooltipActionFieldDescriptor);
        m_tooltipField->setText("{event.extendedDescription}");

        const auto manifest = NotificationAction::manifest();

        m_sourceTextField = makeField(manifest, utils::kSourceNameFieldName);
        m_deviceIdsField = makeField(manifest, utils::kDeviceIdsFieldName);
    }

    std::unique_ptr<ActionBuilderField> makeField(
        const ItemDescriptor& manifest, const QString& type)
    {
        const auto fieldIt = std::ranges::find_if(
            manifest.fields,
            [&type](const auto& f) { return f.fieldName == type; });

        if (fieldIt == manifest.fields.end())
            return {};

        auto field = engine()->buildActionField(&*fieldIt);
        field->setProperties(fieldIt->properties);

        return field;
    }

    void thenNotificationIs(
        const QString& expectedCaption,
        const QString& expectedNotificationDescription,
        const QString& expectedPushDescription,
        const QString& expectedTooltip)
    {
        using nx::vms::rules::utils::EventLog;

        const auto event = buildEvent();

        const auto caption = m_captionField->build(event).value<QString>();
        const auto notificationDescription = m_descriptionField->build(event).value<QString>();
        const auto pushDescription = m_pushDescriptionField->build(event).value<QString>();
        const auto tooltip = m_tooltipField->build(event).value<QString>();

        EXPECT_EQ(expectedCaption, caption);
        EXPECT_EQ(expectedNotificationDescription.trimmed(), notificationDescription);
        EXPECT_EQ(expectedPushDescription.trimmed(), pushDescription.trimmed());
        EXPECT_EQ(expectedTooltip.trimmed(), tooltip);

        // Event tile on event panel should be identical to the notification.
        constexpr auto kInfoLevel = Qn::RI_NameOnly;
        const auto& details = event->details(systemContext(), kInfoLevel);

        EXPECT_EQ(expectedCaption, EventLog::caption(details));
        EXPECT_EQ(expectedNotificationDescription.trimmed(), EventLog::tileDescription(details));
        EXPECT_EQ(
            expectedTooltip.trimmed(),
            EventLog::descriptionTooltip(event, systemContext(), kInfoLevel));
    }

    void thenSourceIs(
        ResourceType expectedType, const UuidList& expectedIds, QString expectedText = {})
    {
        using nx::vms::rules::utils::EventLog;

        constexpr auto kInfoLevel = Qn::RI_WithUrl;
        const auto event = buildEvent();
        const auto& details = event->details(systemContext(), kInfoLevel);
        const auto notificationDeviceIds = m_deviceIdsField->build(event).value<UuidList>();
        const auto notificatonServerId = event->property(utils::kServerIdFieldName).value<Uuid>();

        if (expectedText.isEmpty() && !expectedIds.empty())
            expectedText = Strings::resource(systemContext(), expectedIds.front(), kInfoLevel);

        EXPECT_EQ(expectedText, m_sourceTextField->build(buildEvent()).value<QString>());
        EXPECT_EQ(expectedText, EventLog::sourceText(systemContext(), details));

        EXPECT_EQ(expectedType, EventLog::sourceResourceType(details));
        EXPECT_EQ(expectedIds, EventLog::sourceResourceIds(details));

        if (expectedType == ResourceType::device)
            EXPECT_EQ(expectedIds, notificationDeviceIds);
        else if (expectedType == ResourceType::server)
            EXPECT_EQ(expectedIds.front(), notificatonServerId);
    }

protected:
    std::unique_ptr<TextWithFields> m_captionField;
    std::unique_ptr<TextWithFields> m_descriptionField;
    std::unique_ptr<TextWithFields> m_pushDescriptionField;
    std::unique_ptr<TextWithFields> m_tooltipField;

    std::unique_ptr<ActionBuilderField> m_sourceTextField;
    std::unique_ptr<ActionBuilderField> m_deviceIdsField;
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
        ASSERT_EQ(descriptionField->properties["text"].toString(), m_pushDescriptionField->text());
    }
}

TEST_F(NotificationActionsTest, content_eventsPerDayLimit)
{
    static constexpr auto kExpectedCaption = "Server Started";

    static const auto kExpectedPushDescription = sourceTag("Server1");

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
    thenNotificationIs(kExpectedCaption, {}, kExpectedPushDescription, kExpectedTooltip);
}

//-------------------------------------------------------------------------------------------------
// Analytics Event

TEST_F(NotificationActionsTest, event_analytics)
{
    static const QString kEventTypeId = "nx.LineCrossing";

    static constexpr auto kExpectedCaption = "Caption 1";
    static constexpr auto kExpectedDescription = "Description 1";
    static const auto kExpectedPushDescription =
        descriptionWithSourceTag(kExpectedDescription, "Entrance");

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

    thenNotificationIs(
        kExpectedCaption, kExpectedDescription, kExpectedPushDescription, kExpectedTooltip);

    thenSourceIs(ResourceType::device, {kCamera1Id});
}

TEST_F(NotificationActionsTest, event_analytics_descriptionOnly)
{
    static const QString kEventTypeId = "nx.LineCrossing";

    static constexpr auto kExpectedCaption = "Analytics Event";
    static constexpr auto kExpectedDescription = "Description 1";
    static const auto kExpectedPushDescription =
        descriptionWithSourceTag(kExpectedDescription, "Entrance");

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

    thenNotificationIs(
        kExpectedCaption, kExpectedDescription, kExpectedPushDescription, kExpectedTooltip);

    thenSourceIs(ResourceType::device, {kCamera1Id});
}

TEST_F(NotificationActionsTest, event_analytics_captionOnly)
{
    static const QString kEventTypeId = "nx.LineCrossing";

    static constexpr auto kExpectedCaption = "Caption 1";
    static const auto kExpectedPushDescription = sourceTag("Entrance");
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

    thenNotificationIs(kExpectedCaption, {}, kExpectedPushDescription, kExpectedTooltip);
    thenSourceIs(ResourceType::device, {kCamera1Id});
}

//-------------------------------------------------------------------------------------------------
// Analytics Object

TEST_F(NotificationActionsTest, event_analyticsObject)
{
    static const nx::Uuid kEngine2Id = nx::Uuid::createUuid();
    static const QString kObjectTypeId1 = "nx.LicensePlate";
    static const QString kObjectTypeId2 = "nx.Face";

    static constexpr auto kExpectedCaption = "Object detected";
    static const auto kExpectedPushDescription = sourceTag("Entrance");
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

    thenNotificationIs(kExpectedCaption, {}, kExpectedPushDescription, kExpectedTooltip);
    thenSourceIs(ResourceType::device, {kCamera1Id});
}

//-------------------------------------------------------------------------------------------------
// Camera Input

TEST_F(NotificationActionsTest, event_cameraInput_C112754)
{
    RecordProperty("testrail", "C112754");

    static constexpr auto kExpectedCaption = "Input Signal on Camera";
    static constexpr auto kExpectedDescription = "Input Port: Port_1";
    static const auto kExpectedPushDescription =
        descriptionWithSourceTag(kExpectedDescription, "Entrance");

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

    thenNotificationIs(
        kExpectedCaption, kExpectedDescription, kExpectedPushDescription, kExpectedTooltip);

    thenSourceIs(ResourceType::device, {kCamera1Id});
}

//-------------------------------------------------------------------------------------------------
// Device Disconnected

TEST_F(NotificationActionsTest, event_deviceDisconnected_C112752)
{
    RecordProperty("testrail", "C112752");

    static constexpr auto kExpectedCaption = "Camera disconnected";
    static const auto kExpectedPushDescription = sourceTag("Entrance");
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

    // Device name is already displayed in the source.
    thenNotificationIs(kExpectedCaption, {}, kExpectedPushDescription, kExpectedTooltip);
    thenSourceIs(ResourceType::device, {kCamera1Id});
}

//-------------------------------------------------------------------------------------------------
// Device IP Conflict

TEST_F(NotificationActionsTest, event_deviceIpConflict_C112756)
{
    RecordProperty("testrail", "C112756");

    static constexpr auto kExpectedCaption = "Camera IP Conflict";

    static constexpr auto kExpectedDescription = R"(
Conflicting Address: 10.0.0.1
Camera #1: Entrance (2a:cf:c7:04:c7:c9)
Camera #2: Camera2 (51:dc:54:02:3a:8e))";

    static const auto kExpectedPushDescription =
        descriptionWithSourceTag(kExpectedDescription, "Server1");

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

    thenNotificationIs(
        kExpectedCaption, kExpectedDescription, kExpectedPushDescription, kExpectedTooltip);

    thenSourceIs(ResourceType::server, {kServerId});
}

//-------------------------------------------------------------------------------------------------
// Fan Error

TEST_F(NotificationActionsTest, event_fanError)
{
    static constexpr auto kExpectedCaption = "Fan Failure";
    static const auto kExpectedPushDescription = sourceTag("Server1");

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

    thenNotificationIs(kExpectedCaption, {}, kExpectedPushDescription, kExpectedTooltip);
    thenSourceIs(ResourceType::server, {kServerId});
}

//-------------------------------------------------------------------------------------------------
// Generic Event

TEST_F(NotificationActionsTest, event_generic)
{
    static constexpr auto kExpectedCaption = "Caption 1";
    static constexpr auto kExpectedDescription = "Description 1";
    static const auto kExpectedPushDescription =
        descriptionWithSourceTag(kExpectedDescription, "Source 1");

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

    thenNotificationIs(
        kExpectedCaption, kExpectedDescription, kExpectedPushDescription, kExpectedTooltip);

    thenSourceIs(ResourceType::device, {kCamera1Id, kCamera2Id}, "Source 1");
}

TEST_F(NotificationActionsTest, event_generic_captionOnly)
{
    static constexpr auto kExpectedCaption = "Caption 1";
    static const auto kExpectedPushDescription = sourceTag("Source 1");

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

    thenNotificationIs(kExpectedCaption, {}, kExpectedPushDescription, kExpectedTooltip);
    thenSourceIs(ResourceType::device, {kCamera1Id, kCamera2Id}, "Source 1");
}

TEST_F(NotificationActionsTest, event_generic_descriptionOnly)
{
    static constexpr auto kExpectedCaption = "Generic Event";
    static constexpr auto kExpectedDescription = "Description 1";
    static const auto kExpectedPushDescription =
        descriptionWithSourceTag(kExpectedDescription, "Source 1");

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

    thenNotificationIs(
        kExpectedCaption, kExpectedDescription, kExpectedPushDescription, kExpectedTooltip);

    thenSourceIs(ResourceType::device, {kCamera1Id, kCamera2Id}, "Source 1");
}

TEST_F(NotificationActionsTest, event_generic_1device_nosource)
{
    const QString kExpectedCaption = "Caption 2";
    const QString kExpectedDescription = "Description 1";
    static const auto kExpectedPushDescription =
        descriptionWithSourceTag(kExpectedDescription, "Camera2");

    static constexpr auto kExpectedTooltip = R"(
Generic Event at Server1
15:46:29
  Caption: Caption 2
  Description: Description 1
  Related cameras:
  - Camera2 (10.0.0.2)
)";

    givenEvent(QSharedPointer<GenericEvent>::create(
        makeTimestamp("15:46:29"),
        State::instant,
        /*caption*/ kExpectedCaption,
        /*description*/ kExpectedDescription,
        /*source*/ QString(),
        kServerId,
        UuidList{kCamera2Id}));

    thenNotificationIs(
        kExpectedCaption, kExpectedDescription, kExpectedPushDescription, kExpectedTooltip);

    thenSourceIs(ResourceType::device, {kCamera2Id});
}

TEST_F(NotificationActionsTest, event_generic_2device_nosource)
{
    const QString kExpectedCaption = "Caption 2";
    const QString kExpectedDescription = "Description 1";
    static const auto kExpectedPushDescription =
        descriptionWithSourceTag(kExpectedDescription, "Entrance");

    static constexpr auto kExpectedTooltip = R"(
Generic Event at Server1
15:46:29
  Caption: Caption 2
  Description: Description 1
  Related cameras:
  - Entrance (10.0.0.1)
  - Camera2 (10.0.0.2)
)";

    givenEvent(QSharedPointer<GenericEvent>::create(
        makeTimestamp("15:46:29"),
        State::instant,
        /*caption*/ kExpectedCaption,
        /*description*/ kExpectedDescription,
        /*source*/ QString(),
        kServerId,
        UuidList{kCamera1Id, kCamera2Id}));

    thenNotificationIs(
        kExpectedCaption, kExpectedDescription, kExpectedPushDescription, kExpectedTooltip);

    thenSourceIs(ResourceType::device, {kCamera1Id, kCamera2Id});
}

//-------------------------------------------------------------------------------------------------
// LDAP Sync Issue

TEST_F(NotificationActionsTest, event_ldapSyncIssue)
{
    static constexpr auto kExpectedCaption = "LDAP Sync Issue";
    static constexpr auto kExpectedDescription =
        "Failed to complete the sync within a 5 minutes timeout.";
    static const auto kExpectedPushDescription =
        descriptionWithSourceTag(kExpectedDescription, "Server1");

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

    thenNotificationIs(
        kExpectedCaption, kExpectedDescription, kExpectedPushDescription, kExpectedTooltip);

    thenSourceIs(ResourceType::server, {kServerId});
}

//-------------------------------------------------------------------------------------------------
// License Issue

TEST_F(NotificationActionsTest, event_licenseIssue_C112760)
{
    RecordProperty("testrail", "C112760");

    static constexpr auto kExpectedCaption = "License Issue";

    static constexpr auto kExpectedDescription = R"(
Recording has been disabled on the following cameras:
- Entrance (10.0.0.1))";
    static const auto kExpectedPushDescription =
        descriptionWithSourceTag(kExpectedDescription, "Server1");

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

    thenNotificationIs(
        kExpectedCaption, kExpectedDescription, kExpectedPushDescription, kExpectedTooltip);

    thenSourceIs(ResourceType::server, {kServerId});
}

//-------------------------------------------------------------------------------------------------
// Camera Motion

TEST_F(NotificationActionsTest, event_motion_C112753)
{
    RecordProperty("testrail", "C112753");

    static constexpr auto kExpectedCaption = "Motion on Camera";
    static const auto kExpectedPushDescription = sourceTag("Entrance");

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

    thenNotificationIs(kExpectedCaption, {}, kExpectedPushDescription, kExpectedTooltip);

    thenSourceIs(ResourceType::device, {kCamera1Id});
}

//-------------------------------------------------------------------------------------------------
// Network Issue

TEST_F(NotificationActionsTest, event_networkIssue)
{
    static constexpr auto kExpectedCaption = "Network Issue";

    static constexpr auto kExpectedDescription = R"(
Multicast address conflict detected. Address 10.0.0.1:5555 is already in use by testcamera on secondary stream.)";
    static const auto kExpectedPushDescription =
        descriptionWithSourceTag(kExpectedDescription, "Entrance");

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

    thenNotificationIs(
        kExpectedCaption, kExpectedDescription, kExpectedPushDescription, kExpectedTooltip);

    thenSourceIs(ResourceType::device, {kCamera1Id});
}

//-------------------------------------------------------------------------------------------------
// Integration Diagnostic

TEST_F(NotificationActionsTest, event_integrationDiagnostic_deviceAgent)
{
    static constexpr auto kExpectedCaption = "Caption 1";
    static constexpr auto kExpectedDescription = "Description 1";
    static const auto kExpectedPushDescription =
        descriptionWithSourceTag(kExpectedDescription, "Entrance");

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

    thenNotificationIs(
        kExpectedCaption, kExpectedDescription, kExpectedPushDescription, kExpectedTooltip);

    thenSourceIs(ResourceType::device, {kCamera1Id});
}


TEST_F(NotificationActionsTest, event_integrationDiagnostic_deviceAgent_captionOnly)
{
    static constexpr auto kExpectedCaption = "Caption 1";
    static const auto kExpectedPushDescription = sourceTag("Entrance");
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
    thenNotificationIs(kExpectedCaption, {}, kExpectedPushDescription, kExpectedTooltip);
    thenSourceIs(ResourceType::device, {kCamera1Id});
}

TEST_F(NotificationActionsTest, event_integrationDiagnostic_deviceAgent_descriptionOnly)
{
    static constexpr auto kExpectedCaption = "Integration Diagnostic";
    static constexpr auto kExpectedDescription = "Description 1";
    static const auto kExpectedPushDescription =
        descriptionWithSourceTag(kExpectedDescription, "Entrance");
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

    thenNotificationIs(
        kExpectedCaption, kExpectedDescription, kExpectedPushDescription, kExpectedTooltip);

    thenSourceIs(ResourceType::device, {kCamera1Id});
}

TEST_F(NotificationActionsTest, event_integrationDiagnostic_plugin)
{
    static constexpr auto kExpectedCaption = "Caption 1";
    static constexpr auto kExpectedDescription = "Description 1";
    static const auto kExpectedPushDescription =
        descriptionWithSourceTag(kExpectedDescription, "Engine1");
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

    thenNotificationIs(
        kExpectedCaption, kExpectedDescription, kExpectedPushDescription, kExpectedTooltip);

    thenSourceIs(ResourceType::analyticsEngine, {kEngine1Id});
}

//-------------------------------------------------------------------------------------------------
// POE Over Budget

TEST_F(NotificationActionsTest, event_poeOverBudget)
{
    static constexpr auto kExpectedCaption = "PoE Over Budget";

    static constexpr auto kExpectedDescription = R"(
Current power consumption: 0.5 watts
Upper consumption limit: 0.3 watts
Lower consumption limit: 0.2 watts)";
    static const auto kExpectedPushDescription =
        descriptionWithSourceTag(kExpectedDescription, "Server1");

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

    thenNotificationIs(
        kExpectedCaption, kExpectedDescription, kExpectedPushDescription, kExpectedTooltip);

    thenSourceIs(ResourceType::server, {kServerId});
}

//-------------------------------------------------------------------------------------------------
// SaaS Issue

TEST_F(NotificationActionsTest, event_saasIssue)
{
    static constexpr auto kExpectedCaption = "Services Issue";

    static constexpr auto kExpectedDescription = R"(
Failed to migrate licenses.
- key1
- key2)";
    static const auto kExpectedPushDescription =
        descriptionWithSourceTag(kExpectedDescription, "Server1");

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

    thenNotificationIs(
        kExpectedCaption, kExpectedDescription, kExpectedPushDescription, kExpectedTooltip);

    thenSourceIs(ResourceType::server, {kServerId});
}

//-------------------------------------------------------------------------------------------------
// Server Certificate Error

TEST_F(NotificationActionsTest, event_serverCertificateError)
{
    static constexpr auto kExpectedCaption = "Server Certificate Error";
    static const auto kExpectedPushDescription = sourceTag("Server1");
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

    thenNotificationIs(kExpectedCaption, {}, kExpectedPushDescription, kExpectedTooltip);

    thenSourceIs(ResourceType::server, {kServerId});
}

//-------------------------------------------------------------------------------------------------
// Server Conflict

TEST_F(NotificationActionsTest, event_serverConflict_C112758)
{
    RecordProperty("testrail", "C112758");

    static constexpr auto kExpectedCaption = "Server Conflict";

    static constexpr auto kExpectedDescription = R"(
Discovered a server with the same ID in the same local network:
Server: 10.0.1.15)";
    static const auto kExpectedPushDescription =
        descriptionWithSourceTag(kExpectedDescription, "Server1");

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

    thenNotificationIs(
        kExpectedCaption, kExpectedDescription, kExpectedPushDescription, kExpectedTooltip);

    thenSourceIs(ResourceType::server, {kServerId});
}

//-------------------------------------------------------------------------------------------------
// Server Failure

TEST_F(NotificationActionsTest, event_serverFailure_C112757)
{
    RecordProperty("testrail", "C112757");

    static constexpr auto kExpectedCaption = "Server Failure";
    static constexpr auto kExpectedDescription = "Server stopped unexpectedly.";
    static const auto kExpectedPushDescription =
        descriptionWithSourceTag(kExpectedDescription, "Server1");

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

    thenNotificationIs(
        kExpectedCaption, kExpectedDescription, kExpectedPushDescription, kExpectedTooltip);

    thenSourceIs(ResourceType::server, {kServerId});
}

//-------------------------------------------------------------------------------------------------
// Server Started

TEST_F(NotificationActionsTest, event_serverStarted_C112759)
{
    RecordProperty("testrail", "C112759");

    static constexpr auto kExpectedCaption = "Server Started";
    static const auto kExpectedPushDescription = sourceTag("Server1");
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

    thenNotificationIs(kExpectedCaption, {}, kExpectedPushDescription, kExpectedTooltip);

    thenSourceIs(ResourceType::server, {kServerId});
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
    static constexpr auto kExpectedDescription = "User: User1";
    static const auto kExpectedPushDescription =
        descriptionWithSourceTag(kExpectedDescription, "Entrance");

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

    thenNotificationIs(
        kExpectedCaption, kExpectedDescription, kExpectedPushDescription, kExpectedTooltip);

    thenSourceIs(ResourceType::device, {kCamera1Id});
}

//-------------------------------------------------------------------------------------------------
// Storage Issue

TEST_F(NotificationActionsTest, event_storageIssue_C112755)
{
    RecordProperty("testrail", "C112755");

    static constexpr auto kExpectedCaption = "Storage Issue";
    static constexpr auto kExpectedDescription = R"(System disk "C" is almost full.)";
    static const auto kExpectedPushDescription =
        descriptionWithSourceTag(kExpectedDescription, "Server1");

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

    thenNotificationIs(
        kExpectedCaption, kExpectedDescription, kExpectedPushDescription, kExpectedTooltip);

    thenSourceIs(ResourceType::server, {kServerId});
}

} // namespace nx::vms::rules::test
