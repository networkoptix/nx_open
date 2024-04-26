// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "push_notification_action.h"

#include <nx/utils/qt_helpers.h>
#include <nx/vms/api/data/user_group_data.h>

#include "../action_builder_fields/event_devices_field.h"
#include "../action_builder_fields/flag_field.h"
#include "../action_builder_fields/target_user_field.h"
#include "../action_builder_fields/text_with_fields.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/string_helper.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& PushNotificationAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<PushNotificationAction>(),
        .displayName = tr("Send Mobile Notification"),
        .executionTargets = ExecutionTarget::servers,
        .targetServers = TargetServers::serverWithPublicIp,
        .fields = {
            makeFieldDescriptor<TargetUserField>(
                utils::kUsersFieldName,
                tr("To"),
                {},
                ResourceFilterFieldProperties{
                    .visible = false,
                    .acceptAll = false,
                    .ids = nx::utils::toQSet(vms::api::kAllPowerUserGroupIds),
                    .allowEmptySelection = false,
                    .validationPolicy = kCloudUserValidationPolicy
                }.toVariantMap()),
            makeFieldDescriptor<TextWithFields>("caption", tr("Header"), QString(),
                {
                    { "text", "{@EventCaption}" }
                }),
            makeFieldDescriptor<TextWithFields>("description", tr("Body"), QString(),
                {
                    { "text", "{@EventDescription}" }
                }),
            makeFieldDescriptor<ActionFlagField>("addSource", tr("Add Source Device name to Body")),
            utils::makeIntervalFieldDescriptor(tr("Interval of Action")),

            makeFieldDescriptor<EventDevicesField>(utils::kDeviceIdsFieldName, "Event devices"),
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
    result.insert(utils::kDestinationDetailName, utils::StringHelper(context).subjects(users()));

    return result;
}

} // namespace nx::vms::rules
