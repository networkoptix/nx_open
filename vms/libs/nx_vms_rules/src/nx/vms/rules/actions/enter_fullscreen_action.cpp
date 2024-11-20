// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "enter_fullscreen_action.h"

#include "../action_builder_fields/target_device_field.h"
#include "../action_builder_fields/target_layouts_field.h"
#include "../action_builder_fields/target_users_field.h"
#include "../strings.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& EnterFullscreenAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<EnterFullscreenAction>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Set to Fullscreen")),
        .description = "Expand given camera to fullscreen mode "
            "if it is displayed on the current layout.",
        .flags = ItemFlag::instant,
        .executionTargets = ExecutionTarget::clients,
        .fields = {
            makeFieldDescriptor<TargetDeviceField>(
                utils::kCameraIdFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Camera")),
                {},
                TargetSingleDeviceFieldProperties{
                    .base = FieldProperties{.optional = false},
                    .validationPolicy = kCameraFullScreenValidationPolicy
                }.toVariantMap()),
            makeFieldDescriptor<TargetLayoutsField>(utils::kLayoutIdsFieldName,
                Strings::onLayout(), {}, FieldProperties{.optional = false}.toVariantMap()),
            makeFieldDescriptor<TargetUsersField>(
                utils::kUsersFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Set for")),
                {},
                ResourceFilterFieldProperties{
                    .base = FieldProperties{.visible = false},
                    .acceptAll = true
                }.toVariantMap()),
            utils::makePlaybackFieldDescriptor(Strings::rewind()),
        },
        .resources = {
            {utils::kCameraIdFieldName, {ResourceType::device}},
            {utils::kLayoutIdsFieldName, {ResourceType::layout}},
            {utils::kUsersFieldName, {ResourceType::user}},
        },
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
