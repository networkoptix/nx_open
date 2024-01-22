// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_output_action.h"

#include "../action_builder_fields/optional_time_field.h"
#include "../action_builder_fields/output_port_field.h"
#include "../action_builder_fields/target_device_field.h"
#include "../utils/field.h"
#include "../utils/type.h"

using namespace std::chrono_literals;

namespace nx::vms::rules {

const ItemDescriptor& DeviceOutputAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<DeviceOutputAction>(),
        .displayName = tr("Device Output"),
        .flags = ItemFlag::prolonged,
        .fields = {
            makeFieldDescriptor<TargetDeviceField>(utils::kDeviceIdsFieldName, tr("At")),
            makeFieldDescriptor<OutputPortField>("outputPortId", tr("Output ID")),
            utils::makeTimeFieldDescriptor<OptionalTimeField>(
                utils::kDurationFieldName,
                tr("Duration"),
                {},
                {.initialValue = 1s, .defaultValue = 1s, .minimumValue = 1s}),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
