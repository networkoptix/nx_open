// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "exit_fullscreen_action.h"

#include "../action_builder_fields/target_layouts_field.h"
#include "../action_builder_fields/target_users_field.h"
#include "../strings.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& ExitFullscreenAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<ExitFullscreenAction>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Exit Fullscreen")),
        .description = "Exit fullscreen mode for the specified layouts "
            "for all users if it is currently displayed.",
        .flags = ItemFlag::instant,
        .executionTargets = ExecutionTarget::clients,
        .fields = {
                makeFieldDescriptor<TargetLayoutsField>(utils::kLayoutIdsFieldName,
                    Strings::onLayout(),
                    {},
                    FieldProperties{.optional = false}.toVariantMap()),
            makeFieldDescriptor<TargetUsersField>(
                utils::kUsersFieldName,
                Strings::to(),
                /*description*/ {},
                ResourceFilterFieldProperties{
                    .base = FieldProperties{.visible = false},
                    .acceptAll = true
                }.toVariantMap()),
        },
        .resources = {
            {utils::kLayoutIdsFieldName, {ResourceType::layout}},
            {utils::kUsersFieldName, {ResourceType::user}},
        },
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
