// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/action_builder_fields/target_device_field.h>
#include <nx/vms/rules/action_builder_fields/target_user_field.h>
#include <nx/vms/rules/action_builder_fields/text_with_fields.h>
#include <nx/vms/rules/basic_action.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/type.h>

namespace nx::vms::rules::test {

class TestAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.test.instant")

public:
    static ItemDescriptor manifest()
    {
        return ItemDescriptor{
            .id = utils::type<TestAction>(),
            .displayName = "Test action",
        };
    }
};

class TestProlongedAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.test.prolonged")

    Q_PROPERTY(UuidList deviceIds MEMBER m_deviceIds)

public:
    static ItemDescriptor manifest()
    {
        return ItemDescriptor{
            .id = utils::type<TestProlongedAction>(),
            .displayName = "Test prolonged action",
            .flags = ItemFlag::prolonged,
            .fields = {
                makeFieldDescriptor<TargetDeviceField>(utils::kDeviceIdsFieldName, "Cameras")},
        };
    }

public:
    UuidList m_deviceIds;
};

class TestActionWithInterval: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.test.withInterval")

    Q_PROPERTY(std::chrono::microseconds interval MEMBER m_interval)
    Q_PROPERTY(nx::Uuid cameraId MEMBER m_cameraId)

public:
    static ItemDescriptor manifest()
    {
        return ItemDescriptor{
            .id = utils::type<TestActionWithInterval>(),
            .displayName = "Test action with interval",
            .flags = ItemFlag::instant,
            .fields = {
                utils::makeIntervalFieldDescriptor("Throttle")},
        };
    }

public:
    nx::Uuid m_cameraId;
    std::chrono::microseconds m_interval;
};

class TestActionWithTargetUsers: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.test.withTargetUsers")

    Q_PROPERTY(nx::vms::rules::UuidSelection users MEMBER m_users)
    Q_PROPERTY(nx::Uuid cameraId MEMBER m_cameraId)
    Q_PROPERTY(UuidList deviceIds MEMBER m_deviceIds)

public:
    static ItemDescriptor manifest()
    {
        return ItemDescriptor{
            .id = utils::type<TestActionWithTargetUsers>(),
            .displayName = "Test action with users",
            .flags = ItemFlag::instant,
            .fields = {
                makeFieldDescriptor<TargetUserField>(utils::kUsersFieldName, "Users")},
        };
    }

public:
    nx::vms::rules::UuidSelection m_users;
    nx::Uuid m_cameraId;
    UuidList m_deviceIds;
};

class TestActionWithPermissions: public TestActionWithTargetUsers
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.test.withPermissions")

public:
    static ItemDescriptor manifest()
    {
        return ItemDescriptor{
            .id = utils::type<TestActionWithPermissions>(),
            .displayName = "Test action with permissions",
            .flags = ItemFlag::instant,
            .fields = {
                makeFieldDescriptor<TargetUserField>(utils::kUsersFieldName, "Users"),
                makeFieldDescriptor<TargetDeviceField>(
                    utils::kDeviceIdsFieldName, "Devices", {}, {{"useSource", true}})},
            .permissions = {
                .globalPermission = GlobalPermission::generateEvents,
                .resourcePermissions = {
                    {utils::kCameraIdFieldName, Qn::WritePermission},
                    {utils::kDeviceIdsFieldName, Qn::WritePermission},
                },
            },
        };
    }
};

class TestActionForUserAndServer: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.test.user&server")

    Q_PROPERTY(nx::vms::rules::UuidSelection users MEMBER m_users)

public:
    static ItemDescriptor manifest()
    {
        return ItemDescriptor{
            .id = utils::type<TestActionForUserAndServer>(),
            .displayName = "Test action for user & server",
            .flags = {ItemFlag::instant, ItemFlag::executeOnClientAndServer},
            .fields = {
                makeFieldDescriptor<TargetUserField>(utils::kUsersFieldName, "Users")},
        };
    }

public:
    nx::vms::rules::UuidSelection m_users;
};

class TestActionWithTextWithFields: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.test.withTextWithFields")

    Q_PROPERTY(QString text MEMBER m_text)

public:
    static constexpr auto kFieldName = "text";
    static ItemDescriptor manifest()
    {
        return ItemDescriptor{
            .id = utils::type<TestActionWithTextWithFields>(),
            .displayName = "Test action for text with fields",
            .flags = {ItemFlag::prolonged},
            .fields = {
                makeFieldDescriptor<TextWithFields>(kFieldName, tr("Text with fields"))
            }
        };
    }

    QString m_text;
};

} // namespace nx::vms::rules::test
