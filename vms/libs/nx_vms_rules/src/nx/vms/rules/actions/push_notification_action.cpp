// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "push_notification_action.h"

#include <nx/utils/qt_helpers.h>
#include <nx/vms/api/data/user_group_data.h>

#include "../action_builder_fields/event_devices_field.h"
#include "../action_builder_fields/flag_field.h"
#include "../action_builder_fields/target_user_field.h"
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
        .executionTargets = {ExecutionTarget::clients, ExecutionTarget::cloud},
        .fields = {
            makeFieldDescriptor<TargetUserField>(
                utils::kUsersFieldName,
                Strings::to(),
                {},
                ResourceFilterFieldProperties{
                    .acceptAll = false,
                    .ids = nx::utils::toQSet(vms::api::kAllPowerUserGroupIds),
                    .validationPolicy = kCloudUserValidationPolicy
                }.toVariantMap()),
            makeFieldDescriptor<TextWithFields>(
                utils::kCaptionFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Header")),
                /*description*/ {},
                {
                    { "text", "{event.caption}" }
                }),
            makeFieldDescriptor<TextWithFields>(
                utils::kDescriptionFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Body")),
                /*description*/ {},
                {
                    { "text", "{event.description}" }
                }),
            makeFieldDescriptor<ActionFlagField>(
                "addSource",
                NX_DYNAMIC_TRANSLATABLE(tr("Add Source Device name to Body"))),
            utils::makeIntervalFieldDescriptor(
                Strings::intervalOfAction()),
            makeFieldDescriptor<EventDevicesField>(
                utils::kDeviceIdsFieldName,
                Strings::eventDevices()),
            utils::makeExtractDetailFieldDescriptor("level", utils::kLevelDetailName),
        },
        .resources = {
            {utils::kDeviceIdsFieldName, {ResourceType::device}},
            {utils::kServerIdFieldName, {ResourceType::server}},
        },
    };
    return kDescriptor;
}

QVariantMap PushNotificationAction::details(common::SystemContext* context) const
{
    auto result = BasicAction::details(context);
    result.insert(utils::kDestinationDetailName, Strings::subjects(context, users()));
    return result;
}

} // namespace nx::vms::rules
