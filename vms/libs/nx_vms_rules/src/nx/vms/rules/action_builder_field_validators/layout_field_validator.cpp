// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_field_validator.h"

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_builder_fields/layout_field.h>
#include <nx/vms/rules/action_builder_fields/target_user_field.h>
#include <nx/vms/rules/rule.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/resource.h>

#include "../rule.h"
#include "../strings.h"
#include "../utils/validity.h"

namespace nx::vms::rules {

ValidationResult LayoutFieldValidator::validity(
    const Field* field, const Rule* rule, common::SystemContext* context) const
{
    auto layoutField = dynamic_cast<const LayoutField*>(field);
    if (!NX_ASSERT(layoutField))
        return {QValidator::State::Invalid, Strings::invalidFieldType()};

    const auto layout =
        context->resourcePool()->getResourceById<QnLayoutResource>(layoutField->value());
    if (!layout)
        return {QValidator::State::Invalid, tr("Select layout")};

    auto targetUserField =
        rule->actionBuilders().front()->fieldByName<TargetUserField>(utils::kUsersFieldName);
    if (targetUserField)
    {
        const auto users = utils::users(targetUserField->selection(), context);
        return utils::layoutValidity(context, layout, users.values());
    }

    return {};
}

} // namespace nx::vms::rules
