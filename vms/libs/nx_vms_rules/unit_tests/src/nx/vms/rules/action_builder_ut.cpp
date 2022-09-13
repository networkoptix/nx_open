// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/vms/common/resource/property_adaptors.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>
#include <nx/vms/common/test_support/test_context.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_fields/optional_time_field.h>
#include <nx/vms/rules/action_fields/target_user_field.h>
#include <nx/vms/rules/aggregated_event.h>
#include <nx/vms/rules/basic_action.h>
#include <nx/vms/rules/basic_event.h>
#include <nx/vms/rules/events/server_failure_event.h>
#include <nx/vms/rules/events/server_started_event.h>
#include <nx/vms/rules/utils/field.h>
#include <utils/common/synctime.h>

#include "mock_action_builder_events.h"
#include "test_action.h"
#include "test_plugin.h"
#include "test_router.h"

namespace nx::vms::rules::test {

using nx::vms::api::GlobalPermission;

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
    ActionBuilderTest() : TestPlugin(engine.get())
    {
        registerEvent<ServerStartedEvent>();
        registerEvent<ServerFailureEvent>();
    }

    QSharedPointer<ActionBuilder> makeSimpleBuilder() const
    {
        return QSharedPointer<ActionBuilder>::create(
            QnUuid::createUuid(),
            utils::type<TestAction>(),
            []{ return new TestAction; });
    }

    QSharedPointer<TestActionBuilder> makeTestActionBuilder(
        const std::function<BasicAction*()>& actionConstructor) const
    {
        return QSharedPointer<TestActionBuilder>::create(
            QnUuid::createUuid(),
            utils::type<TestActionWithTargetUsers>(),
            actionConstructor);
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

    QSharedPointer<TestActionBuilder> makeBuilderWithIntervalField(
        std::chrono::microseconds interval) const
    {
        auto builder = makeTestActionBuilder([]{ return new TestAction; });

        auto intervalField = std::make_unique<OptionalTimeField>();
        intervalField->setValue(interval);

        builder->addField(utils::kIntervalFieldName, std::move(intervalField));

        return builder;
    }

    TestEventPtr makeSimpleEvent() const
    {
        return TestEventPtr::create(syncTime.currentTimePoint());
    }

    TestEventPtr makeEventWithCameraId(const QnUuid& cameraId) const
    {
        auto event = makeSimpleEvent();
        event->m_cameraId = cameraId;
        return event;
    }

    TestEventPtr makeEventWithDeviceIds(const QnUuidList& deviceIds) const
    {
        auto event = makeSimpleEvent();
        event->m_deviceIds = deviceIds;
        return event;
    }

private:
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
    const auto user = addUser(nx::vms::api::GlobalPermission::userInput);

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
    const auto user1 = addUser(nx::vms::api::GlobalPermission::userInput);
    const auto user2 = addUser(nx::vms::api::GlobalPermission::userInput);

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
    const auto user1 = addUser(nx::vms::api::GlobalPermission::userInput);
    const auto user2 = addUser(nx::vms::api::GlobalPermission::userInput);

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
    const auto admin = addUser(nx::vms::api::GlobalPermission::adminPermissions);
    const auto user = addUser(nx::vms::api::GlobalPermission::accessAllMedia);
    const auto camera = addCamera();

    auto adminRoleId = QnUserRolesManager::predefinedRoleId(nx::vms::api::GlobalPermission::adminPermissions);

    UuidSelection selection{
        .ids = {*adminRoleId},
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
    const auto user = addUser(nx::vms::api::GlobalPermission::userInput);
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

    const auto accessAllMediaUser = addUser(
        {GlobalPermission::accessAllMedia, GlobalPermission::userInput});
    const auto accessNoneUser = addUser(nx::vms::api::GlobalPermission::userInput);

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
    auto user1 = addUser(GlobalPermission::userInput);
    auto cameraOfUser1 = addCamera();
    sharedResourcesManager()->setSharedResources(user1, {cameraOfUser1->getId()});

    auto user2 = addUser(GlobalPermission::userInput);
    auto cameraOfUser2 = addCamera();
    sharedResourcesManager()->setSharedResources(user2, {cameraOfUser2->getId()});

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

    AggregationInfoList aggregationInfoList;
    aggregationInfoList.push_back({makeEventWithCameraId(cameraOfUser1->getId()), 1});
    aggregationInfoList.push_back({makeEventWithCameraId(cameraOfUser2->getId()), 1});

    auto eventAggregator = AggregatedEventPtr::create(aggregationInfoList);

    EXPECT_EQ(eventAggregator->uniqueCount(), 2);

    builder->process(eventAggregator);
}

TEST_F(ActionBuilderTest, eventDevicesAreFiltered)
{
    auto user1 = addUser(GlobalPermission::userInput);
    auto cameraOfUser1 = addCamera();
    sharedResourcesManager()->setSharedResources(user1, {cameraOfUser1->getId()});

    auto user2 = addUser(GlobalPermission::userInput);
    auto cameraOfUser2 = addCamera();
    sharedResourcesManager()->setSharedResources(user2, {cameraOfUser2->getId()});

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

    EXPECT_EQ(eventAggregator->uniqueCount(), 1);

    builder->process(eventAggregator);
}

TEST_F(ActionBuilderTest, eventWithoutDevicesIsProcessed)
{
    auto user1 = addUser(GlobalPermission::userInput);

    UuidSelection selection{ .all = true };
    auto builder = makeBuilderWithTargetUserField(selection);
    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(1);

    UuidSelection expectedSelection1{
        .ids = {user1->getId()},
        .all = false};
    EXPECT_CALL(mock, targetedUsers(expectedSelection1)).Times(1);

    auto eventAggregator = AggregatedEventPtr::create(makeSimpleEvent());

    EXPECT_EQ(eventAggregator->uniqueCount(), 1);

    builder->process(eventAggregator);
}

TEST_F(ActionBuilderTest, userWithoutPermissionsReceivesNoAction)
{
    auto user1 = addUser(GlobalPermission::none);

    UuidSelection selection{.ids = {user1->getId()}};
    auto builder = makeBuilderWithTargetUserField(selection);
    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(0);

    auto eventAggregator = AggregatedEventPtr::create(makeSimpleEvent());
    builder->process(eventAggregator);
}

TEST_F(ActionBuilderTest, userEventFilterPropertyWorks)
{
    using nx::vms::event::EventType;
    using namespace std::chrono;

    auto user1 = addUser(GlobalPermission::admin);
    auto filterProp = nx::vms::common::BusinessEventFilterResourcePropertyAdaptor();

    filterProp.setResource(user1);
    filterProp.setWatchedEvents({EventType::serverStartEvent});

    UuidSelection selection{.ids = {user1->getId()}};
    auto builder = makeBuilderWithTargetUserField(selection);
    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(1);

    builder->process(AggregatedEventPtr::create(EventPtr(
        new ServerStartedEvent(0ms, QnUuid()))));
    builder->process(AggregatedEventPtr::create(EventPtr(
        new ServerFailureEvent(0ms, QnUuid(), nx::vms::api::EventReason::none))));
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

TEST_F(ActionBuilderTest, builderWithIntervalAggregatesEvents)
{
    const std::chrono::milliseconds interval(100);
    auto builder = makeBuilderWithIntervalField(interval);

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

TEST_F(ActionBuilderTest, builderWithIntervalAggregatesAndEmitsThemWhenTimeElapsed)
{
    const std::chrono::milliseconds interval(100);
    auto builder = makeBuilderWithIntervalField(interval);

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
    auto builder = makeBuilderWithIntervalField(interval);

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
