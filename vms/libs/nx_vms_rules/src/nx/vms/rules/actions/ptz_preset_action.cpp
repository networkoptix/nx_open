// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ptz_preset_action.h"

#include "../action_builder_fields/ptz_preset_field.h"
#include "../action_builder_fields/target_single_device_field.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& PtzPresetAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<PtzPresetAction>(),
        .displayName = tr("Execute PTZ preset"),
        .flags = ItemFlag::instant,
        .fields = {
            makeFieldDescriptor<TargetSingleDeviceField>(utils::kCameraIdFieldName, tr("Camera")),
            utils::makeIntervalFieldDescriptor(tr("Interval of action")),
            makeFieldDescriptor<PtzPresetField>("presetId", tr("PTZ preset")),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
