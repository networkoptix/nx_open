// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "show_on_alarm_layout_action.h"

#include <nx/utils/qt_helpers.h>
#include <nx/vms/api/data/user_group_data.h>

#include "../action_builder_fields/event_devices_field.h"
#include "../action_builder_fields/event_id_field.h"
#include "../action_builder_fields/target_device_field.h"
#include "../action_builder_fields/target_user_field.h"
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
        .executionTargets = ExecutionTarget::clients,
        .fields = {
            makeFieldDescriptor<TargetDeviceField>(utils::kDeviceIdsFieldName, {}),
            makeFieldDescriptor<TargetUserField>(utils::kUsersFieldName,
                Strings::to(),
                {},
                ResourceFilterFieldProperties{
                    .acceptAll = false,
                    .ids = nx::utils::toQSet(vms::api::kAllPowerUserGroupIds),
                    .allowEmptySelection = false,
                    .validationPolicy = {}
                }.toVariantMap()),
            utils::makePlaybackFieldDescriptor(Strings::rewind()),
            utils::makeActionFlagFieldDescriptor(
                "forceOpen",
                NX_DYNAMIC_TRANSLATABLE(tr("Force Alarm Layout Opening")),
                {},
                /*defaultValue*/ true),
            utils::makeIntervalFieldDescriptor(Strings::intervalOfAction()),

            makeFieldDescriptor<EventDevicesField>("eventDeviceIds", {}),
            utils::makeTextFormatterFieldDescriptor(utils::kCaptionFieldName,
                tr("Alarm: %1").arg("{@EventCaption}")),
            utils::makeTextFormatterFieldDescriptor(utils::kDescriptionFieldName,
                "{@EventDescription}"),
            utils::makeTextFormatterFieldDescriptor("tooltip",
                "{@ExtendedEventDescription}"),
            utils::makeExtractDetailFieldDescriptor("sourceName",
                utils::kSourceNameDetailName),
        },
        .resources = {
            {utils::kDeviceIdsFieldName, {ResourceType::device}},
            {utils::kServerIdFieldName, {ResourceType::server}},
        },
    };
    return kDescriptor;
}

ShowOnAlarmLayoutAction::ShowOnAlarmLayoutAction()
{
    setLevel(nx::vms::event::Level::critical);
}

QVariantMap ShowOnAlarmLayoutAction::details(common::SystemContext* context) const
{
    auto result = BasicAction::details(context);
    result.insert(utils::kDestinationDetailName, Strings::subjects(context, users()));
    return result;
}

} // namespace nx::vms::rules
