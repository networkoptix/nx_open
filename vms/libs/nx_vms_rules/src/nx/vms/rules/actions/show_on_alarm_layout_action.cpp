// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "show_on_alarm_layout_action.h"

#include <nx/utils/qt_helpers.h>
#include <nx/vms/api/data/user_group_data.h>

#include "../action_builder_fields/event_devices_field.h"
#include "../action_builder_fields/target_devices_field.h"
#include "../action_builder_fields/target_users_field.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& ShowOnAlarmLayoutAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<ShowOnAlarmLayoutAction>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Show on Alarm Layout")),
        .description = "Put the given device(s) to the Alarm Layout.",
        .executionTargets = ExecutionTarget::clients,
        .fields = {
            makeFieldDescriptor<TargetDevicesField>(utils::kDeviceIdsFieldName,
                {},
                {},
                ResourceFilterFieldProperties{.base = FieldProperties{.optional = false}}
                    .toVariantMap()),
            makeFieldDescriptor<TargetUsersField>(utils::kUsersFieldName,
                Strings::to(),
                "By default, all power user group IDs are used as the value.",
                ResourceFilterFieldProperties{
                    .ids = nx::utils::toQSet(vms::api::kAllPowerUserGroupIds)
                }.toVariantMap()),
            utils::makeActionFlagFieldDescriptor("forceOpen",
                NX_DYNAMIC_TRANSLATABLE(tr("Force Alarm Layout Opening")),
                "Specifies whether the layout must be forcibly opened or not.",
                /*defaultValue*/ true),
            utils::makePlaybackFieldDescriptor(Strings::rewind()),
            utils::makeIntervalFieldDescriptor(Strings::intervalOfAction()),

            makeFieldDescriptor<EventDevicesField>("eventDeviceIds", {}),
            utils::makeNotificationTextWithFieldsDescriptor(
                utils::kCaptionFieldName, /* isVisibilityConfigurable */ true,
                tr("Alarm: %1").arg("{event.caption}")),
            utils::makeNotificationTextWithFieldsDescriptor(
                utils::kDescriptionFieldName, /* isVisibilityConfigurable */ true),
            utils::makeNotificationTextWithFieldsDescriptor(
                utils::kTooltipFieldName, /* isVisibilityConfigurable */ true),
            utils::makeExtractDetailFieldDescriptor("sourceName",
                utils::kSourceNameDetailName),
        },
        .resources = {
            {utils::kDeviceIdsFieldName, {ResourceType::device, Qn::ViewContentPermission}},
            {utils::kUsersFieldName, {ResourceType::user}},
        },
    };
    return kDescriptor;
}

ShowOnAlarmLayoutAction::ShowOnAlarmLayoutAction()
{
    setLevel(nx::vms::event::Level::critical);
}

} // namespace nx::vms::rules
