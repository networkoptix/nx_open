// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gmock/gmock.h>

#include <nx/utils/qt_helpers.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/basic_action.h>

#include "test_action.h"

namespace nx::vms::rules::test {

class MockActionBuilderEvents: public QObject
{
    Q_OBJECT

public:
    MOCK_METHOD(void, actionReceived, ());
    MOCK_METHOD(void, targetedUsers, (const UuidSelection& selection));
    MOCK_METHOD(void, targetedCameraId, (nx::Uuid id));
    MOCK_METHOD(void, targetedDeviceIds, (UuidList ids));

    MockActionBuilderEvents(ActionBuilder* builder):
        m_builder(builder)
    {
        connect(m_builder, &ActionBuilder::action, this,
            &MockActionBuilderEvents::onAction, Qt::DirectConnection);
    }

    ActionPtr action(size_t i) { return m_actions[i]; };

private:
    ActionBuilder* m_builder;
    std::vector<ActionPtr> m_actions;

    void onAction(const ActionPtr& action)
    {
        m_actions.push_back(action);

        actionReceived();

        if (const auto prop = action->property(utils::kUsersFieldName);
            prop.canConvert<UuidSelection>())
        {
            targetedUsers(prop.value<UuidSelection>());
        }

        if (auto targetedAction = action.dynamicCast<TestActionWithTargetUsers>())
        {
            if (!targetedAction->m_cameraId.isNull())
                targetedCameraId(targetedAction->m_cameraId);

            if (!targetedAction->m_deviceIds.empty())
                targetedDeviceIds(targetedAction->m_deviceIds);
        }
    }
};

class MockPermissionsActionEvents: public QObject
{
    Q_OBJECT

public:
    MOCK_METHOD(void, multiDeviceAction, (const UuidSet& usersIds, const UuidSet& devices));
    MOCK_METHOD(void, singleDeviceAction, (const UuidSet& usersIds, nx::Uuid device));

    MockPermissionsActionEvents(ActionBuilder* builder):
        m_builder(builder)
    {
        connect(m_builder, &ActionBuilder::action, this,
            &MockPermissionsActionEvents::onAction, Qt::DirectConnection);
    }

private:
    ActionBuilder* m_builder;

    void onAction(const ActionPtr& value)
    {
        auto action = value.dynamicCast<TestActionWithPermissions>();
        if (!action)
            return;

        if (action->m_deviceIds.isEmpty())
            singleDeviceAction(action->m_users.ids, action->m_cameraId);
        else
            multiDeviceAction(action->m_users.ids, nx::utils::toQSet(action->m_deviceIds));
    }
};

} // namespace nx::vms::rules::test
