// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "push_notification_action.h"

#include "../action_builder_fields/event_devices_field.h"
#include "../action_builder_fields/flag_field.h"
#include "../action_builder_fields/target_user_field.h"
#include "../action_builder_fields/text_with_fields.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& PushNotificationAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<PushNotificationAction>(),
        .displayName = tr("Send mobile notification"),
        .fields = {
            makeFieldDescriptor<TargetUserField>(utils::kUsersFieldName, tr("To")),
            utils::makeIntervalFieldDescriptor(tr("Interval of action")),
            makeFieldDescriptor<TextWithFields>("caption", tr("Header"), QString(),
                {
                    { "text", "{@EventCaption}" }
                }),
            makeFieldDescriptor<TextWithFields>("description", tr("Body"), QString(),
                {
                    { "text", "{@EventDescription}" }
                }),
            makeFieldDescriptor<FlagField>("addSource", tr("Add source device name in body")),

            makeFieldDescriptor<EventDevicesField>(utils::kDeviceIdsFieldName, "Event devices"),
            utils::makeExtractDetailFieldDescriptor("level", utils::kLevelDetailName),
            }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
