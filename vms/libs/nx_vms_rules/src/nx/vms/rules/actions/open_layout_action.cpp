// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "open_layout_action.h"

#include "../action_builder_fields/target_layout_field.h"
#include "../action_builder_fields/target_users_field.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& OpenLayoutAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<OpenLayoutAction>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Open Layout")),
        .flags = ItemFlag::instant,
        .executionTargets = ExecutionTarget::clients,
        .fields = {
            makeFieldDescriptor<TargetUsersField>(
                utils::kUsersFieldName,
                Strings::to(),
                {},
                ResourceFilterFieldProperties{
                    .base = FieldProperties{.optional = false},
                    .validationPolicy = kLayoutAccessValidationPolicy
                }.toVariantMap()),
            makeFieldDescriptor<TargetLayoutField>(
                utils::kLayoutIdFieldName, {}, {},
                FieldProperties{.optional = false}.toVariantMap()),
            utils::makePlaybackFieldDescriptor(Strings::rewind()),
            utils::makeIntervalFieldDescriptor(Strings::intervalOfAction()),
        },
        .resources = {
            {utils::kLayoutIdFieldName, {ResourceType::layout, Qn::ReadPermission}},
            {utils::kUsersFieldName, {ResourceType::user}},
        },
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
