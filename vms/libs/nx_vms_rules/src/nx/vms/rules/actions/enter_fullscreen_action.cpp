// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "enter_fullscreen_action.h"

#include "../action_builder_fields/target_layout_field.h"
#include "../action_builder_fields/target_single_device_field.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& EnterFullscreenAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<EnterFullscreenAction>(),
        .displayName = tr("Set to Fullscreen"),
        .flags = ItemFlag::instant,
        .executionTargets = ExecutionTarget::clients,
        .fields = {
            makeFieldDescriptor<TargetSingleDeviceField>(
                utils::kCameraIdFieldName, tr("Camera"), {}, {}, {utils::kLayoutIdsFieldName}),
            makeFieldDescriptor<TargetLayoutField>(utils::kLayoutIdsFieldName, tr("On Layout")),
            utils::makeTargetUserFieldDescriptor(
                tr("Set for"), {}, utils::UserFieldPreset::All, /*visible*/ false),
            utils::makePlaybackFieldDescriptor(tr("Rewind")),
        },
        .resources = {
            {utils::kCameraIdFieldName, {ResourceType::device}},
            {utils::kLayoutIdsFieldName, {ResourceType::layout}},
        },
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
