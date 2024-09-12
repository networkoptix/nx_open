// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/action_builder_fields/email_message_field.h>
#include <nx/vms/rules/action_builder_fields/optional_time_field.h>
#include <nx/vms/rules/action_builder_fields/target_devices_field.h>
#include <nx/vms/rules/action_builder_fields/target_users_field.h>
#include <nx/vms/rules/action_builder_fields/text_with_fields.h>
#include <nx/vms/rules/basic_action.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/type.h>
#include <utils/email/message.h>

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
            .displayName = TranslatableString("Test action"),
        };
    }
};

class TestProlongedAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.test.prolonged")

    Q_PROPERTY(UuidList deviceIds MEMBER m_deviceIds)
    Q_PROPERTY(std::chrono::microseconds duration MEMBER m_duration)

public:
    static ItemDescriptor manifest()
    {
        return ItemDescriptor{
            .id = utils::type<TestProlongedAction>(),
            .displayName = TranslatableString("Test prolonged action"),
            .flags = ItemFlag::prolonged,
            .fields = {
                makeFieldDescriptor<TargetDevicesField>(
                    utils::kDeviceIdsFieldName,
                    TranslatableString("Cameras")),
                utils::makeTimeFieldDescriptor<OptionalTimeField>(
                    utils::kDurationFieldName, nx::TranslatableString("Duration")),
            },
        };
    }

public:
    UuidList m_deviceIds;
    std::chrono::microseconds m_duration;
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
            .displayName = TranslatableString("Test action with interval"),
            .flags = ItemFlag::instant,
            .fields = {
                utils::makeIntervalFieldDescriptor(TranslatableString("Throttle"))
            },
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
            .displayName = TranslatableString("Test action with users"),
            .flags = {ItemFlag::instant, ItemFlag::userFiltered},
            .executionTargets = {ExecutionTarget::clients},
            .fields = {
                makeFieldDescriptor<TargetUsersField>(
                    utils::kUsersFieldName,
                    TranslatableString("Users"))
            },
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
            .displayName = TranslatableString("Test action with permissions"),
            .flags = ItemFlag::instant,
            .executionTargets = {ExecutionTarget::clients},
            .fields = {
                makeFieldDescriptor<TargetUsersField>(
                    utils::kUsersFieldName,
                    TranslatableString("Users")),
                makeFieldDescriptor<TargetDevicesField>(
                    utils::kDeviceIdsFieldName,
                    TranslatableString("Devices"),
                    {},
                    {{"useSource", true}})
            },
            .resources = {
                {utils::kCameraIdFieldName, {ResourceType::device, Qn::WritePermission}},
                {utils::kDeviceIdsFieldName, {ResourceType::device, Qn::WritePermission}}},
            .readPermissions = GlobalPermission::generateEvents,
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
            .displayName = TranslatableString("Test action for user & server"),
            .flags = {ItemFlag::instant},
            .executionTargets = {ExecutionTarget::clients, ExecutionTarget::servers},
            .fields = {
                makeFieldDescriptor<TargetUsersField>(
                    utils::kUsersFieldName,
                    TranslatableString("Users"))
            },
        };
    }

public:
    nx::vms::rules::UuidSelection m_users;
};

class TestActionForServerWithTargetUser: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.test.serverWithTargetUser")

    Q_PROPERTY(nx::vms::rules::UuidSelection users MEMBER m_users)

public:
    static ItemDescriptor manifest()
    {
        return ItemDescriptor{
            .id = utils::type<TestActionForServerWithTargetUser>(),
            .displayName = TranslatableString("Test action for server with target user"),
            .flags = {ItemFlag::instant},
            .executionTargets = {ExecutionTarget::servers},
            .fields = {
                makeFieldDescriptor<TargetUsersField>(
                    utils::kUsersFieldName,
                    TranslatableString("Users"))
            },
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
            .displayName = TranslatableString("Test action for text with fields"),
            .flags = {ItemFlag::prolonged},
            .fields = {
                makeFieldDescriptor<TextWithFields>(
                    kFieldName,
                    TranslatableString("Text with fields"))
            }
        };
    }

    QString m_text;
};

class TestActionWithEmail: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.test.withEmailMessage")

    Q_PROPERTY(nx::email::Message message MEMBER m_message)

public:
    static ItemDescriptor manifest()
    {
        return ItemDescriptor{
            .id = utils::type<TestActionWithTextWithFields>(),
            .displayName = TranslatableString("Test action for email field"),
            .fields = {
                makeFieldDescriptor<EmailMessageField>(
                    utils::kMessageFieldName,
                    TranslatableString("Email message field"))
            }
        };
    }

    nx::email::Message m_message;
};

} // namespace nx::vms::rules::test
