// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "buzzer_action.h"

#include "../action_builder_fields/optional_time_field.h"
#include "../action_builder_fields/target_server_field.h"
#include "../utils/field.h"
#include "../utils/type.h"

using namespace std::chrono_literals;

namespace nx::vms::rules {

const ItemDescriptor& BuzzerAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<BuzzerAction>(),
        .displayName = tr("Buzzer"),
        .flags = ItemFlag::prolonged,
        .fields = {
            makeFieldDescriptor<TargetServerField>(utils::kServerIdsFieldName, tr("Servers")),
            utils::makeIntervalFieldDescriptor(tr("Interval of action")),
            utils::makeTimeFieldDescriptor<OptionalTimeField>(
                utils::kDurationFieldName,
                tr("Fixed duration"),
                {},
                {.initialValue = 1s, .defaultValue = 1s, .maximumValue = 24h, .minimumValue = 1s})
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
