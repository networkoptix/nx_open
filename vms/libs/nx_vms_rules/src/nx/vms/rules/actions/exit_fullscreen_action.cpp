// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "exit_fullscreen_action.h"

#include "../action_builder_fields/target_layout_field.h"
#include "../action_builder_fields/target_user_field.h"
#include "../strings.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& ExitFullscreenAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<ExitFullscreenAction>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Exit Fullscreen")),
        .flags = ItemFlag::instant,
        .executionTargets = ExecutionTarget::clients,
        .fields = {
            makeFieldDescriptor<TargetLayoutField>(utils::kLayoutIdsFieldName,
                Strings::onLayout()),
            makeFieldDescriptor<TargetUserField>(
                utils::kUsersFieldName,
                Strings::to(),
                /*description*/ {},
                ResourceFilterFieldProperties{
                    .visible = false,
                    .acceptAll = true
                }.toVariantMap()),
        },
        .resources = {{utils::kLayoutIdsFieldName, {ResourceType::layout}}},
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
