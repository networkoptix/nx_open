// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "enter_fullscreen_action.h"

#include "../action_builder_fields/optional_time_field.h"
#include "../action_builder_fields/target_layout_field.h"
#include "../action_builder_fields/target_single_device_field.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& EnterFullscreenAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<EnterFullscreenAction>(),
        .displayName = tr("Set to fullscreen"),
        .flags = ItemFlag::instant,
        .fields = {
            makeFieldDescriptor<TargetSingleDeviceField>(
                utils::kCameraIdFieldName, tr("Camera"), {}, {}, {utils::kLayoutIdsFieldName}),
            makeFieldDescriptor<TargetLayoutField>(utils::kLayoutIdsFieldName, tr("On Layout")),
            makeFieldDescriptor<OptionalTimeField>(utils::kPlaybackTimeFieldName, tr("Playback Time")),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
