// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>
#include <nx/vms/common/test_support/test_context.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_fields/target_user_field.h>
#include <nx/vms/rules/basic_action.h>
#include <nx/vms/rules/basic_event.h>
#include <nx/vms/rules/event_aggregator.h>
#include <nx/vms/rules/utils/field.h>

#include "mock_action_builder_events.h"
#include "test_action.h"
#include "test_event.h"

namespace nx::vms::rules::test {

class TestActionBuilder: public ActionBuilder
{
public:
    using ActionBuilder::ActionBuilder;
    using ActionBuilder::process;

    void process(const EventAggregatorPtr& eventAggregator)
    {
        m_eventAggregator = eventAggregator;
        buildAndEmitAction();
    }
};

class ActionBuilderTest: public common::test::ContextBasedTest
{
public:
    QSharedPointer<ActionBuilder> makeSimpleBuilder() const
    {
        return QSharedPointer<ActionBuilder>::create(
            QnUuid::createUuid(),
            utils::type<TestAction>(),
            []{ return new TestAction; });
    }

    QSharedPointer<TestActionBuilder> makeBuilderWithTargetUserField(
        const UuidSelection& selection) const
    {
        auto builder = QSharedPointer<TestActionBuilder>::create(
            QnUuid::createUuid(),
            utils::type<TestActionWithTargetUsers>(),
            []{ return new TestActionWithTargetUsers; });

        auto targetUserField = std::make_unique<TargetUserField>(systemContext());
        targetUserField->setIds(selection.ids);
        targetUserField->setAcceptAll(selection.all);

        builder->addField(utils::kUsersFieldName, std::move(targetUserField));

        return builder;
    }

    EventPtr makeSimpleEvent() const
    {
        return QSharedPointer<BasicEvent>::create(std::chrono::microseconds::zero());
    }

    EventPtr makeEventWithCameraId(const QnUuid& cameraId) const
    {
        auto event = QSharedPointer<TestCameraEvent>::create(std::chrono::microseconds::zero());
        event->m_cameraId = cameraId;
        return event;
    }
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

TEST_F(ActionBuilderTest, builderWithOneTargetUserProducedOnceAction)
{
    const auto user = addUser(nx::vms::api::GlobalPermission::accessAllMedia);

    UuidSelection selection{
        .ids = {user->getId()},
        .all = false};

    auto builder = makeBuilderWithTargetUserField(selection);

    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(1);
    EXPECT_CALL(mock, targetedUsers(selection)).Times(1);

    builder->process(makeSimpleEvent());
}

TEST_F(ActionBuilderTest, builderWithTwoTargetUserProducedTwoAction)
{
    const auto user1 = addUser(nx::vms::api::GlobalPermission::accessAllMedia);
    const auto user2 = addUser(nx::vms::api::GlobalPermission::accessAllMedia);

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

    UuidSelection expectedSelection2{
        .ids = {user2->getId()},
        .all = false};
    EXPECT_CALL(mock, targetedUsers(expectedSelection2)).Times(1);

    builder->process(makeSimpleEvent());
}

TEST_F(ActionBuilderTest, builderProperlyHandleAllUsersSelection)
{
    const auto user1 = addUser(nx::vms::api::GlobalPermission::accessAllMedia);
    const auto user2 = addUser(nx::vms::api::GlobalPermission::accessAllMedia);

    UuidSelection selection{
        .ids = {},
        .all = true};

    auto builder = makeBuilderWithTargetUserField(selection);

    MockActionBuilderEvents mock{builder.get()};

    EXPECT_CALL(mock, actionReceived()).Times(2);

    UuidSelection expectedSelection1{
        .ids = {user1->getId()},
        .all = false};
    EXPECT_CALL(mock, targetedUsers(expectedSelection1)).Times(1);

    UuidSelection expectedSelection2{
        .ids = {user2->getId()},
        .all = false};
    EXPECT_CALL(mock, targetedUsers(expectedSelection2)).Times(1);

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
    const auto user = addUser(nx::vms::api::GlobalPermission::none);
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

    const auto accessAllMediaUser = addUser(nx::vms::api::GlobalPermission::accessAllMedia);
    const auto accessNoneUser = addUser(nx::vms::api::GlobalPermission::none);

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
    auto user1 = addUser(GlobalPermission::none);
    auto cameraOfUser1 = addCamera();
    sharedResourcesManager()->setSharedResources(user1, {cameraOfUser1->getId()});

    auto user2 = addUser(GlobalPermission::none);
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

    auto eventAggregator = EventAggregatorPtr::create(makeEventWithCameraId(cameraOfUser1->getId()));
    eventAggregator->aggregate(makeEventWithCameraId(cameraOfUser2->getId()));

    EXPECT_EQ(eventAggregator->uniqueCount(), 2);

    builder->process(eventAggregator);
}

} // namespace nx::vms::rules::test
