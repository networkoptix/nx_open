// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "enter_fullscreen_action.h"

#include "../action_builder_fields/target_layout_field.h"
#include "../action_builder_fields/target_single_device_field.h"
#include "../action_builder_fields/target_user_field.h"
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
            makeFieldDescriptor<TargetSingleDeviceField>(utils::kCameraIdFieldName, tr("Camera")),
            makeFieldDescriptor<TargetLayoutField>(utils::kLayoutIdsFieldName, tr("On Layout")),
            makeFieldDescriptor<TargetUserField>(
                utils::kUsersFieldName,
                tr("Set for"),
                {},
                ResourceFilterFieldProperties{
                    .visible = false,
                    .acceptAll = true,
                    .ids = {},
                    .allowEmptySelection = false,
                    .validationPolicy = {}
                }.toVariantMap()),
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
