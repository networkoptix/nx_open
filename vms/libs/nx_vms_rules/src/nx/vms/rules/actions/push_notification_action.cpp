// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "push_notification_action.h"

#include <nx/utils/qt_helpers.h>
#include <nx/vms/api/data/user_group_data.h>

#include "../action_builder_fields/event_devices_field.h"
#include "../action_builder_fields/flag_field.h"
#include "../action_builder_fields/target_users_field.h"
#include "../action_builder_fields/text_with_fields.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& PushNotificationAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<PushNotificationAction>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Send Mobile Notification")),
        .description = "Sends a mobile notification via the cloud.",
        .flags = {ItemFlag::instant, ItemFlag::eventPermissions},
        .executionTargets = {ExecutionTarget::clients, ExecutionTarget::cloud},
        .fields = {
            makeFieldDescriptor<TargetUsersField>(
                utils::kUsersFieldName,
                Strings::to(),
                "Cloud users, who should receive the notification."
                "By default, all power user group IDs are used as the value.",
                ResourceFilterFieldProperties{
                    .acceptAll = false,
                    .ids = nx::utils::toQSet(vms::api::kAllPowerUserGroupIds),
                    .validationPolicy = kCloudUserValidationPolicy
                }.toVariantMap()),
            makeFieldDescriptor<TextWithFields>(
                utils::kCaptionFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Header")),
                "Notification header",
                {
                    { "text", "{event.caption}" }
                }),
            makeFieldDescriptor<TextWithFields>(
                utils::kDescriptionFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Body")),
                "Notification body.",
                {
                    { "text", "{event.description}" }
                }),
            makeFieldDescriptor<ActionFlagField>(
                "addSource",
                NX_DYNAMIC_TRANSLATABLE(tr("Add Source Device name to Body")),
                "Specifies whether the source "
                "device name must be added to the body or not."),
            utils::makeIntervalFieldDescriptor(
                Strings::intervalOfAction()),
            makeFieldDescriptor<EventDevicesField>(
                utils::kDeviceIdsFieldName,
                Strings::eventDevices()),
            utils::makeExtractDetailFieldDescriptor("level", utils::kLevelDetailName),
        },
        .resources = {
            {utils::kUsersFieldName, {ResourceType::user}},
        },
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
