// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gmock/gmock.h>

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
    MOCK_METHOD(void, targetedCameraId, (QnUuid id));

    MockActionBuilderEvents(ActionBuilder* builder):
        m_builder(builder)
    {
        connect(m_builder, &ActionBuilder::action, this, &MockActionBuilderEvents::onAction, Qt::DirectConnection);
    }

private:
    ActionBuilder* m_builder;

    void onAction(const ActionPtr& action)
    {
        actionReceived();

        if (auto targetedAction = action.dynamicCast<TestActionWithTargetUsers>())
        {
            targetedUsers(targetedAction->m_users);
            if (!targetedAction->m_cameraId.isNull())
                targetedCameraId(targetedAction->m_cameraId);
        }
    }
};

} // namespace nx::vms::rules::test
