// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "open_layout_action.h"

#include "../action_builder_fields/layout_field.h"
#include "../action_builder_fields/target_user_field.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

QVariantMap OpenLayoutAction::details(common::SystemContext* context) const
{
    auto result = BasicAction::details(context);
    result.insert(utils::kDestinationDetailName, Strings::subjects(context, users()));
    return result;
}

const ItemDescriptor& OpenLayoutAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<OpenLayoutAction>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Open Layout")),
        .flags = ItemFlag::instant,
        .executionTargets = ExecutionTarget::clients,
        .fields = {
            makeFieldDescriptor<LayoutField>(utils::kLayoutIdFieldName, {}),
            makeFieldDescriptor<TargetUserField>(
                utils::kUsersFieldName,
                Strings::to(),
                {},
                ResourceFilterFieldProperties{
                    .validationPolicy = kLayoutAccessValidationPolicy
                }.toVariantMap()),
            utils::makePlaybackFieldDescriptor(Strings::rewind()),
            utils::makeIntervalFieldDescriptor(Strings::intervalOfAction()),
        },
        .resources = {{utils::kLayoutIdFieldName, {ResourceType::layout, Qn::ReadPermission}}},
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
