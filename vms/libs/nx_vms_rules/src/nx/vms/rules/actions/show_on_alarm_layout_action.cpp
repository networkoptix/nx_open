// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "show_on_alarm_layout_action.h"

#include "../action_builder_fields/event_devices_field.h"
#include "../action_builder_fields/event_id_field.h"
#include "../action_builder_fields/target_device_field.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/string_helper.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& ShowOnAlarmLayoutAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<ShowOnAlarmLayoutAction>(),
        .displayName = tr("Show on Alarm Layout"),
        .fields = {
            makeFieldDescriptor<TargetDeviceField>(utils::kDeviceIdsFieldName, {}),
            utils::makeTargetUserFieldDescriptor(tr("To")),
            utils::makePlaybackFieldDescriptor(tr("Rewind")),
            utils::makeActionFlagFieldDescriptor(
                "forceOpen", tr("Force Alarm Layout Opening"), {}, /*defaultValue*/ true),
            utils::makeIntervalFieldDescriptor("Interval of Action"),

            makeFieldDescriptor<EventIdField>("id", "Event ID"),
            makeFieldDescriptor<EventDevicesField>("eventDeviceIds", "Event devices"),
            utils::makeTextFormatterFieldDescriptor("caption", tr("Alarm: %1").arg("{@EventCaption}")),
            utils::makeTextFormatterFieldDescriptor("description", "{@EventDescription}"),
            utils::makeTextFormatterFieldDescriptor("tooltip", "{@ExtendedEventDescription}"),
            utils::makeExtractDetailFieldDescriptor("sourceName", utils::kSourceNameDetailName),
        }
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
    result.insert(utils::kDestinationDetailName, utils::StringHelper(context).subjects(users()));

    return result;
}

} // namespace nx::vms::rules
