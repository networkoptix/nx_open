// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <core/resource/user_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <nx/vms/common/resource/property_adaptors.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>
#include <nx/vms/common/test_support/test_context.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_builder_fields/optional_time_field.h>
#include <nx/vms/rules/action_builder_fields/target_device_field.h>
#include <nx/vms/rules/action_builder_fields/target_user_field.h>
#include <nx/vms/rules/aggregated_event.h>
#include <nx/vms/rules/basic_action.h>
#include <nx/vms/rules/basic_event.h>
#include <nx/vms/rules/events/server_failure_event.h>
#include <nx/vms/rules/events/server_started_event.h>
#include <nx/vms/rules/rule.h>
#include <nx/vms/rules/utils/field.h>
#include <utils/common/synctime.h>

#include "mock_action_builder_events.h"
#include "test_action.h"
#include "test_plugin.h"
#include "test_router.h"

namespace nx::vms::rules::test {

using namespace nx::vms::api;

class TestActionBuilder: public ActionBuilder
{
public:
    using ActionBuilder::ActionBuilder;
    using ActionBuilder::process;

    void process(const AggregatedEventPtr& aggregatedEvent)
    {
        buildAndEmitAction(aggregatedEvent);
    }

    using ActionBuilder::processEvent;
    using ActionBuilder::handleAggregatedEvents;
};

class ActionBuilderTest:
    public common::test::ContextBasedTest,
    public TestEngineHolder,
    public TestPlugin
{
public:
    ActionBuilderTest() : TestEngineHolder(context()->systemContext()), TestPlugin(engine.get())
    {
        registerEvent<ServerStartedEvent>();
        registerEvent<ServerFailureEvent>();

        m_engine->registerActionField(
            fieldMetatype<TargetUserField>(),
            [this] { return new TargetUserField(context()->systemContext()); });

        registerAction<TestActionWithTargetUsers>();
        registerAction<TestActionWithPermissions>();
        registerAction<TestActionForUserAndServer>();

        mockRule = std::make_unique<Rule>(QnUuid::createUuid(), engine.get());
    }

    QSharedPointer<ActionBuilder> makeSimpleBuilder() const
    {
        auto builder = QSharedPointer<ActionBuilder>::create(
            QnUuid::createUuid(),
            utils::type<TestAction>(),
            []{ return new TestAction; });
        builder->setRule(mockRule.get());
        return builder;
    }

    QSharedPointer<TestActionBuilder> makeTestActionBuilder(
        const std::function<BasicAction*()>& actionConstructor) const
    {
        NX_ASSERT(actionConstructor);
        auto action = actionConstructor();
        NX_ASSERT(action);

        auto builder = QSharedPointer<TestActionBuilder>::create(
            QnUuid::createUuid(),
            action->type(),
            actionConstructor);
        builder->setRule(mockRule.get());
        return builder;
    }

    QSharedPointer<TestActionBuilder> makeBuilderWithTargetUserField(
        const UuidSelection& selection) const
    {
        auto builder = makeTestActionBuilder([]{ return new TestActionWithTargetUsers; });

        auto targetUserField = std::make_unique<TargetUserField>(systemContext());
        targetUserField->setIds(selection.ids);
        targetUserField->setAcceptAll(selection.all);

        builder->addField(utils::kUsersFieldName, std::move(targetUserField));

        return builder;
    }

    QSharedPointer<TestActionBuilder> makeBuilderWithPermissions(
        const UuidSelection& selection,
        const QnUuidSet& devices = {},
        bool useSource = false) const
    {
        auto builder = makeTestActionBuilder([]{ return new TestActionWithPermissions; });

        auto targetUserField = std::make_unique<TargetUserField>(systemContext());
        targetUserField->setIds(selection.ids);
        targetUserField->setAcceptAll(selection.all);

        auto devicesField = std::make_unique<TargetDeviceField>();
        devicesField->setUseSource(useSource);
        devicesField->setIds(devices);
        devicesField->setAcceptAll(false);

        builder->addField(utils::kUsersFieldName, std::move(targetUserField));
        builder->addField(utils::kDeviceIdsFieldName, std::move(devicesField));

        return builder;
    }

    void addIntervalField(const QSharedPointer<TestActionBuilder>& builder, std::chrono::microseconds interval)
    {
        auto intervalField = std::make_unique<OptionalTimeField>();
        intervalField->setValue(interval);

        builder->addField(utils::kIntervalFieldName, std::move(intervalField));
    }

    void addDurationField(const QSharedPointer<TestActionBuilder>& builder, std::chrono::microseconds duration)
    {
        auto durationField = std::make_unique<OptionalTimeField>();
        durationField->setValue(duration);

        builder->addField(utils::kDurationFieldName, std::move(durationField));
    }

    SimpleEventPtr makeSimpleEvent() const
    {
        return SimpleEventPtr::create(syncTime.currentTimePoint());
    }

    SimpleEventPtr makeSimpleEventWithCamera(QnUuid cameraId) const
    {
        auto event = makeSimpleEvent();
        event->m_cameraId = cameraId;
        return event;
    }

    SimpleEventPtr makeSimpleEventWithDevices(const QnUuidList& deviceIds) const
    {
        auto event = makeSimpleEvent();
        event->m_deviceIds = deviceIds;
        return event;
    }

    TestEventPtr makeTestEvent() const
    {
        return TestEventPtr::create(syncTime.currentTimePoint());
    }

    TestEventPtr makeEventWithCameraId(const QnUuid& cameraId) const
    {
        auto event = makeTestEvent();
        event->m_cameraId = cameraId;
        return event;
    }

    TestEventPtr makeEventWithDeviceIds(const QnUuidList& deviceIds) const
    {
        auto event = makeTestEvent();
        event->m_deviceIds = deviceIds;
        return event;
    }
protected:
    std::unique_ptr<Rule> mockRule;
    QnSyncTime syncTime;
};

TEST_F(ActionBuilderTest, builderWithoutTargetUserFieldProduceOnlyOneAction)
{
    auto builder = makeSimpleBuilder();

    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(1);

    builder->process(makeSimpleEvent());
}

TEST_F(ActionBuilderTest, builderWithTargetFieldButWithoutUserProducedNoAction)
{
    auto builder = makeBuilderWithTargetUserField({});

    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(0);

    builder->process(makeSimpleEvent());
}

TEST_F(ActionBuilderTest, builderWithOneTargetUserProducesOneAction)
{
    const auto user = addUser(NoGroup, kTestUserName, UserType::local, GlobalPermission::viewLogs);

    UuidSelection selection{
        .ids = {user->getId()},
        .all = false};

    auto builder = makeBuilderWithTargetUserField(selection);

    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(1);
    EXPECT_CALL(mock, targetedUsers(selection)).Times(1);

    builder->process(makeSimpleEvent());
}

TEST_F(ActionBuilderTest, builderWithManyUserWithSameRightsProducesOneAction)
{
    const auto user1 = addUser(NoGroup, kTestUserName, UserType::local, GlobalPermission::viewLogs);
    const auto user2 = addUser(NoGroup, kTestUserName, UserType::local, GlobalPermission::viewLogs);

    UuidSelection selection{
        .ids = {user1->getId(), user2->getId()},
        .all = false};

    auto builder = makeBuilderWithTargetUserField(selection);

    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(1);

    UuidSelection expectedSelection1{
        .ids = {user1->getId(), user2->getId()},
        .all = false};
    EXPECT_CALL(mock, targetedUsers(expectedSelection1)).Times(1);

    builder->process(makeSimpleEvent());
}

TEST_F(ActionBuilderTest, builderProperlyHandleAllUsersSelection)
{
    const auto user1 = addUser(NoGroup, kTestUserName, UserType::local, GlobalPermission::viewLogs);
    const auto user2 = addUser(NoGroup, kTestUserName, UserType::local, GlobalPermission::viewLogs);

    UuidSelection selection{
        .ids = {},
        .all = true};

    auto builder = makeBuilderWithTargetUserField(selection);

    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(1);

    UuidSelection expectedSelection{
        .ids = {user1->getId(), user2->getId()},
        .all = false};
    EXPECT_CALL(mock, targetedUsers(expectedSelection)).Times(1);

    builder->process(makeSimpleEvent());
}

TEST_F(ActionBuilderTest, builderProperlyHandleUserRoles)
{
    const auto admin = addUser(kPowerUsersGroupId);
    const auto user = addUser(kLiveViewersGroupId);

    const auto camera = addCamera();

    UuidSelection selection{
        .ids = {kPowerUsersGroupId},
        .all = false};

    auto builder = makeBuilderWithTargetUserField(selection);

    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(1);

    UuidSelection expectedSelection{
        .ids = {admin->getId()},
        .all = false};
    EXPECT_CALL(mock, targetedUsers(expectedSelection)).Times(1);
    EXPECT_CALL(mock, targetedCameraId(camera->getId())).Times(1);

    builder->process(makeEventWithCameraId(camera->getId()));
}

TEST_F(ActionBuilderTest, builderWithTargetUsersWithoutAppropriateRightsProducedNoAction)
{
    const auto user = addUser(NoGroup, kTestUserName, UserType::local, GlobalPermission::viewLogs);
    const auto camera = addCamera();

    UuidSelection selection{
        .ids = {user->getId()},
        .all = false};

    auto builder = makeBuilderWithTargetUserField(selection);

    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(0);

    builder->process(makeEventWithCameraId(camera->getId()));
}

TEST_F(ActionBuilderTest, builderWithTargetUsersProducedActionsOnlyForUsersWithAppropriateRights)
{
    const auto camera = addCamera();

    const auto accessAllMediaUser = addUser(kLiveViewersGroupId, kTestUserName,
        UserType::local, GlobalPermission::viewLogs);

    const auto accessNoneUser = addUser(NoGroup, kTestUserName,
        UserType::local, GlobalPermission::viewLogs);

    UuidSelection selection{
        .ids = {accessAllMediaUser->getId(), accessNoneUser->getId()},
        .all = false};

    auto builder = makeBuilderWithTargetUserField(selection);

    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(1);

    UuidSelection expectedSelection{
        .ids = {accessAllMediaUser->getId()},
        .all = false};
    EXPECT_CALL(mock, targetedUsers(expectedSelection)).Times(1);
    EXPECT_CALL(mock, targetedCameraId(camera->getId())).Times(1);

    builder->process(makeEventWithCameraId(camera->getId()));
}

TEST_F(ActionBuilderTest, usersReceivedActionsWithAppropriateCameraId)
{
    auto cameraOfUser1 = addCamera();
    auto user1 = addUser(NoGroup,
        kTestUserName,
        UserType::local,
        GlobalPermission::viewLogs,
        {{cameraOfUser1->getId(), AccessRight::view}});

    auto cameraOfUser2 = addCamera();
    auto user2 = addUser(NoGroup,
        kTestUserName,
        UserType::local,
        GlobalPermission::viewLogs,
        {{cameraOfUser2->getId(), AccessRight::view}});

    EXPECT_TRUE(resourceAccessManager()->hasPermission(user1, cameraOfUser1, Qn::ViewContentPermission));
    EXPECT_FALSE(resourceAccessManager()->hasPermission(user1, cameraOfUser2, Qn::ViewContentPermission));

    EXPECT_TRUE(resourceAccessManager()->hasPermission(user2, cameraOfUser2, Qn::ViewContentPermission));
    EXPECT_FALSE(resourceAccessManager()->hasPermission(user2, cameraOfUser1, Qn::ViewContentPermission));

    UuidSelection selection{
        .ids = {user1->getId(), user2->getId()},
        .all = false};

    auto builder = makeBuilderWithTargetUserField(selection);

    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(2);

    UuidSelection expectedSelection1{
        .ids = {user1->getId()},
        .all = false};
    EXPECT_CALL(mock, targetedUsers(expectedSelection1)).Times(1);
    EXPECT_CALL(mock, targetedCameraId(cameraOfUser1->getId())).Times(1);

    UuidSelection expectedSelection2{
        .ids = {user2->getId()},
        .all = false};
    EXPECT_CALL(mock, targetedUsers(expectedSelection2)).Times(1);
    EXPECT_CALL(mock, targetedCameraId(cameraOfUser2->getId())).Times(1);

    std::vector<EventPtr> aggregationInfoList{
        makeEventWithCameraId(cameraOfUser1->getId()),
        makeEventWithCameraId(cameraOfUser2->getId())};

    auto eventAggregator = AggregatedEventPtr::create(std::move(aggregationInfoList));

    EXPECT_EQ(eventAggregator->count(), 2);

    builder->process(eventAggregator);
}

TEST_F(ActionBuilderTest, eventDevicesAreFiltered)
{
    auto cameraOfUser1 = addCamera();
    auto user1 = addUser(NoGroup,
        kTestUserName,
        UserType::local,
        GlobalPermission::viewLogs,
        {{cameraOfUser1->getId(), AccessRight::view}});

    auto cameraOfUser2 = addCamera();
    auto user2 = addUser(NoGroup,
        kTestUserName,
        UserType::local,
        GlobalPermission::viewLogs,
        {{cameraOfUser2->getId(), AccessRight::view}});

    UuidSelection selection{
        .ids = {user1->getId(), user2->getId()},
        .all = false};

    auto builder = makeBuilderWithTargetUserField(selection);

    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(2);

    UuidSelection expectedSelection1{
        .ids = {user1->getId()},
        .all = false};
    EXPECT_CALL(mock, targetedUsers(expectedSelection1)).Times(1);
    EXPECT_CALL(mock, targetedDeviceIds(QnUuidList{cameraOfUser1->getId()})).Times(1);

    UuidSelection expectedSelection2{
        .ids = {user2->getId()},
        .all = false};
    EXPECT_CALL(mock, targetedUsers(expectedSelection2)).Times(1);
    EXPECT_CALL(mock, targetedDeviceIds(QnUuidList{cameraOfUser2->getId()})).Times(1);

    auto eventAggregator = AggregatedEventPtr::create(
        makeEventWithDeviceIds({cameraOfUser1->getId(), cameraOfUser2->getId()}));

    EXPECT_EQ(eventAggregator->count(), 1);

    builder->process(eventAggregator);
}

TEST_F(ActionBuilderTest, eventWithoutDevicesIsProcessed)
{
    auto user1 = addUser(NoGroup, kTestUserName, UserType::local, GlobalPermission::viewLogs);

    UuidSelection selection{ .all = true };
    auto builder = makeBuilderWithTargetUserField(selection);
    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(1);

    UuidSelection expectedSelection1{
        .ids = {user1->getId()},
        .all = false};
    EXPECT_CALL(mock, targetedUsers(expectedSelection1)).Times(1);

    auto eventAggregator = AggregatedEventPtr::create(makeSimpleEvent());

    EXPECT_EQ(eventAggregator->count(), 1);

    builder->process(eventAggregator);
}

TEST_F(ActionBuilderTest, userWithoutPermissionsReceivesNoAction)
{
    auto user1 = addUser(NoGroup);

    UuidSelection selection{.ids = {user1->getId()}};
    auto builder = makeBuilderWithTargetUserField(selection);
    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(0);

    auto eventAggregator = AggregatedEventPtr::create(makeTestEvent());
    builder->process(eventAggregator);
}

TEST_F(ActionBuilderTest, userEventFilterPropertyWorks)
{
    using nx::vms::event::EventType;
    using namespace std::chrono;

    auto user1 = addUser(kPowerUsersGroupId);
    auto filterProp = nx::vms::common::BusinessEventFilterResourcePropertyAdaptor();

    filterProp.setResource(user1);
    filterProp.setWatchedEvents({EventType::serverStartEvent});

    UuidSelection selection{.ids = {user1->getId()}};
    auto builder = makeBuilderWithTargetUserField(selection);
    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(1);
    EXPECT_CALL(mock, targetedUsers(selection)).Times(1);

    builder->process(AggregatedEventPtr::create(EventPtr(
        new ServerStartedEvent(0ms, QnUuid()))));
    builder->process(AggregatedEventPtr::create(EventPtr(
        new ServerFailureEvent(0ms, QnUuid(), EventReason::none))));
}

TEST_F(ActionBuilderTest, actionPermissionGlobal)
{
    auto user1 = addUser(NoGroup,
        kTestUserName,
        UserType::local,
        GlobalPermission::generateEvents,
        {{kAllDevicesGroupId, AccessRight::viewArchive}});

    UuidSelection selection{ .all = true };
    auto builder = makeBuilderWithPermissions(selection);
    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(1);

    UuidSelection expectedSelection1{
        .ids = {user1->getId()},
        .all = false};
    EXPECT_CALL(mock, targetedUsers(expectedSelection1)).Times(1);

    auto eventAggregator = AggregatedEventPtr::create(makeSimpleEvent());
    builder->process(eventAggregator);
}

TEST_F(ActionBuilderTest, actionPermissionSingleResource)
{
    auto cameraOfUser1 = addCamera();
    auto user1 = addUser(NoGroup,
        kTestUserName,
        UserType::local,
        GlobalPermission::generateEvents,
        {{cameraOfUser1->getId(), AccessRight::edit}});

    UuidSelection selection{ .all = true };
    auto builder = makeBuilderWithPermissions(selection);
    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(1);

    UuidSelection expectedSelection1{
        .ids = {user1->getId()},
        .all = false};
    EXPECT_CALL(mock, targetedUsers(expectedSelection1)).Times(1);
    EXPECT_CALL(mock, targetedCameraId(cameraOfUser1->getId())).Times(1);

    auto eventAggregator = AggregatedEventPtr::create(makeSimpleEventWithCamera(cameraOfUser1->getId()));
    builder->process(eventAggregator);
}

TEST_F(ActionBuilderTest, actionPermissionResourceFiltration)
{
    auto cameraA = addCamera();
    auto cameraB = addCamera();
    auto cameraC = addCamera();
    auto user1 = addUser(NoGroup,
        kTestUserName,
        UserType::local,
        GlobalPermission::generateEvents,
        {{cameraA->getId(), AccessRight::edit}, {cameraB->getId(), AccessRight::edit}});
    auto user2 = addUser(NoGroup,
        kTestUserName,
        UserType::local,
        GlobalPermission::generateEvents,
        {{cameraB->getId(), AccessRight::edit}, {cameraC->getId(), AccessRight::edit}});

    UuidSelection selection{ .all = true };
    const auto devices = QnUuidSet{cameraA->getId(), cameraB->getId(), cameraC->getId()};
    auto builder = makeBuilderWithPermissions(selection, devices);
    MockPermissionsActionEvents mock{builder.get()};

    EXPECT_CALL(mock, multiDeviceAction(
        QnUuidSet{user1->getId()}, QnUuidSet{cameraA->getId(), cameraB->getId()})).Times(1);
    EXPECT_CALL(mock, multiDeviceAction(
        QnUuidSet{user2->getId()}, QnUuidSet{cameraB->getId(), cameraC->getId()})).Times(1);

    auto eventAggregator = AggregatedEventPtr::create(makeSimpleEvent());
    builder->process(eventAggregator);
}

TEST_F(ActionBuilderTest, actionPermissionUserGrouping)
{
    auto cameraA = addCamera();
    auto cameraB = addCamera();
    auto user1 = addUser(NoGroup,
        kTestUserName,
        UserType::local,
        GlobalPermission::generateEvents,
        {{cameraA->getId(), AccessRight::edit}});
    auto user2 = addUser(NoGroup,
        kTestUserName,
        UserType::local,
        GlobalPermission::generateEvents,
        {{cameraA->getId(), AccessRight::edit}});
    auto user3 = addUser(NoGroup,
        kTestUserName,
        UserType::local,
        GlobalPermission::generateEvents,
        {{cameraB->getId(), AccessRight::edit}});
    auto user4 = addUser(NoGroup,
        kTestUserName,
        UserType::local,
        GlobalPermission::generateEvents,
        {{cameraA->getId(), AccessRight::edit}, {cameraB->getId(), AccessRight::edit}});

    UuidSelection selection{ .all = true };
    auto builder = makeBuilderWithPermissions(selection, {cameraA->getId(), cameraB->getId()});
    MockPermissionsActionEvents mock{builder.get()};

    EXPECT_CALL(mock, multiDeviceAction(
        QnUuidSet{user1->getId(), user2->getId()}, QnUuidSet{cameraA->getId()})).Times(1);
    EXPECT_CALL(mock, multiDeviceAction(
        QnUuidSet{user3->getId()}, QnUuidSet{cameraB->getId()})).Times(1);
    EXPECT_CALL(mock, multiDeviceAction(
        QnUuidSet{user4->getId()}, QnUuidSet{cameraA->getId(), cameraB->getId()})).Times(1);

    auto eventAggregator = AggregatedEventPtr::create(makeSimpleEvent());
    builder->process(eventAggregator);
}

TEST_F(ActionBuilderTest, clientAndServerAction)
{
    const auto user = addUser(NoGroup, kTestUserName, UserType::local, GlobalPermission::viewLogs);

    UuidSelection selection{
        .ids = {user->getId()},
        .all = false};

    auto builder = engine->buildActionBuilder(TestActionForUserAndServer::manifest().id);
    builder->setRule(mockRule.get());

    auto userField = builder->fieldByName<TargetUserField>(utils::kUsersFieldName);
    userField->setIds(selection.ids);

    MockActionBuilderEvents mock{builder.get()};

    // Expecting two action copies. One for target user and one for server processing.
    EXPECT_CALL(mock, actionReceived()).Times(2);
    EXPECT_CALL(mock, targetedUsers(selection)).Times(1);
    EXPECT_CALL(mock, targetedUsers(UuidSelection())).Times(1);

    builder->process(makeSimpleEvent());
}

TEST_F(ActionBuilderTest, builderWithoutIntervalNotAggregatesEvents)
{
    auto builder = makeSimpleBuilder();

    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(3);

    builder->process(makeSimpleEvent());
    builder->process(makeSimpleEvent());
    builder->process(makeSimpleEvent());
}

TEST_F(ActionBuilderTest, builderWithIntervalAggregatesInstantEvents)
{
    const std::chrono::milliseconds interval(100);
    const auto builder = makeTestActionBuilder([]{ return new TestAction; });
    addIntervalField(builder, interval);

    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(1);

    const auto initialEvent = makeSimpleEvent();
    builder->processEvent(initialEvent);

    // The following 2 events are aggregated till the interval not elapsed.
    builder->processEvent(makeSimpleEvent());
    builder->processEvent(makeSimpleEvent());

    // If this check fails, than interval must be increased.
    ASSERT_TRUE(qnSyncTime->currentTimePoint() - initialEvent->timestamp() < interval);
    builder->handleAggregatedEvents();
}

TEST_F(ActionBuilderTest, builderForProlongedActionWithIntervalDoesNotAggregateEvents)
{
    const std::chrono::milliseconds interval(100);
    const auto builder = makeTestActionBuilder([]{ return new TestProlongedAction; });
    addIntervalField(builder, interval);

    MockActionBuilderEvents mock{builder.get()};

    // Action must be received on event start and stop.
    EXPECT_CALL(mock, actionReceived()).Times(2);

    const auto prolongedEvent = SimpleEventPtr::create(syncTime.currentTimePoint(), State::started);
    builder->processEvent(prolongedEvent);

    prolongedEvent->setState(State::stopped);
    builder->processEvent(prolongedEvent);
}

TEST_F(ActionBuilderTest, builderWithIntervalAndFixedDurationAggregateEvents)
{
    const std::chrono::milliseconds interval(100);
    const std::chrono::milliseconds duration(10);
    const auto builder = makeTestActionBuilder([]{ return new TestProlongedAction; });
    addIntervalField(builder, interval);
    addDurationField(builder, duration);

    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(1);

    const auto prolongedEvent = SimpleEventPtr::create(syncTime.currentTimePoint(), State::started);
    builder->processEvent(prolongedEvent);

    // The following events with the State::started must be aggregated.
    builder->processEvent(prolongedEvent);
    builder->processEvent(prolongedEvent);
}

TEST_F(ActionBuilderTest, builderWithIntervalAggregatesAndEmitsThemWhenTimeElapsed)
{
    const std::chrono::milliseconds interval(100);
    const auto builder = makeTestActionBuilder([]{ return new TestAction; });
    addIntervalField(builder, interval);

    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(2);

    builder->processEvent(makeSimpleEvent());
    // The following 2 events are aggregated till the interval not elapsed.
    builder->processEvent(makeSimpleEvent());
    builder->processEvent(makeSimpleEvent());

    std::this_thread::sleep_for(interval);

    // In the real builder this method is called when the timer elapsed.
    builder->handleAggregatedEvents();
}

TEST_F(ActionBuilderTest, builderAggregatesDifferentTypesOfEventProperly)
{
    auto camera1 = addCamera();
    auto camera2 = addCamera();

    const std::chrono::milliseconds interval(100);
    const auto builder = makeTestActionBuilder([]{ return new TestAction; });
    addIntervalField(builder, interval);

    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(4);

    builder->processEvent(makeEventWithCameraId(camera1->getId()));
    builder->processEvent(makeEventWithCameraId(camera1->getId()));
    builder->processEvent(makeEventWithCameraId(camera1->getId()));

    builder->processEvent(makeEventWithCameraId(camera2->getId()));
    builder->processEvent(makeEventWithCameraId(camera2->getId()));
    builder->processEvent(makeEventWithCameraId(camera2->getId()));

    std::this_thread::sleep_for(interval);

    // In the real builder this method is called when the timer elapsed.
    builder->handleAggregatedEvents();
}

} // namespace nx::vms::rules::test
