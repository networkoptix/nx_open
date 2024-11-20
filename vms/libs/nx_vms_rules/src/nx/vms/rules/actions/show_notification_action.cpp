// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "show_notification_action.h"

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

const ItemDescriptor& NotificationAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<NotificationAction>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Show Desktop Notification")),
        .description = "Show Desktop Notification",
        .flags = {ItemFlag::instant, ItemFlag::userFiltered},
        .executionTargets = {ExecutionTarget::clients, ExecutionTarget::cloud},
        .fields = {
            makeFieldDescriptor<TargetUsersField>(
                utils::kUsersFieldName,
                Strings::to(),
                {},
                ResourceFilterFieldProperties{
                    .ids = nx::utils::toQSet(vms::api::kAllPowerUserGroupIds),
                    .validationPolicy = kBookmarkManagementValidationPolicy
                }.toVariantMap()),
            makeFieldDescriptor<ActionFlagField>(utils::kAcknowledgeFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Force Acknowledgement")),
                "Specifies whether the user must manually acknowledge this event "
                "on the event tile and create a bookmark."),
            utils::makeIntervalFieldDescriptor(Strings::intervalOfAction()),
            utils::makeNotificationTextWithFieldsDescriptor(
                utils::kCaptionFieldName, /* isVisibilityConfigurable */ true),
            utils::makeNotificationTextWithFieldsDescriptor(
                utils::kDescriptionFieldName, /* isVisibilityConfigurable */ true),
            utils::makeNotificationTextWithFieldsDescriptor(
                utils::kTooltipFieldName, /* isVisibilityConfigurable */ true),
            makeFieldDescriptor<EventDevicesField>(
                utils::kDeviceIdsFieldName,
                Strings::eventDevices()),
            utils::makeExtractDetailFieldDescriptor(
                "sourceName",
                utils::kSourceNameDetailName),
            utils::makeExtractDetailFieldDescriptor(
                "level",
                utils::kLevelDetailName),
            utils::makeExtractDetailFieldDescriptor(
                "icon",
                utils::kIconDetailName),
            utils::makeExtractDetailFieldDescriptor(
                "customIcon",
                utils::kCustomIconDetailName),
            utils::makeExtractDetailFieldDescriptor(
                "clientAction",
                utils::kClientActionDetailName),
            utils::makeExtractDetailFieldDescriptor(
                utils::kUrlFieldName,
                utils::kUrlDetailName),
            utils::makeExtractDetailFieldDescriptor(
                utils::kExtendedCaptionFieldName,
                utils::kExtendedCaptionDetailName),
        },
        .resources = {
            {utils::kUsersFieldName, {ResourceType::user}},
        },
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
