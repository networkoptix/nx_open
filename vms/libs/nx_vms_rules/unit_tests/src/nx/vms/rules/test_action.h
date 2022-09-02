// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/basic_action.h>
#include <nx/vms/rules/utils/type.h>

namespace nx::vms::rules::test {

class TestAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.testAction")
};

class TestProlongedAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.test.prolonged")

public:
    static ItemDescriptor manifest()
    {
        return ItemDescriptor{
            .id = utils::type<TestProlongedAction>(),
            .displayName = "Test prolonged event",
            .flags = ItemFlag::prolonged,
        };
    }
};

class TestActionWithTargetUsers: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.test.withTargetUsers")

    Q_PROPERTY(nx::vms::rules::UuidSelection users MEMBER m_users)
    Q_PROPERTY(QnUuid cameraId MEMBER m_cameraId)
    Q_PROPERTY(QnUuidList deviceIds MEMBER m_deviceIds)

public:
    nx::vms::rules::UuidSelection m_users;
    QnUuid m_cameraId;
    QnUuidList m_deviceIds;
};

} // namespace nx::vms::rules::test
