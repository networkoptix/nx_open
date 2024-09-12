// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_layout_field_validator.h"

#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/rules/action_builder_fields/target_layouts_field.h>

#include "../rule.h"
#include "../strings.h"

namespace nx::vms::rules {

ValidationResult TargetLayoutFieldValidator::validity(
    const Field* field, const Rule* /*rule*/, common::SystemContext* context) const
{
    auto targetLayoutField = dynamic_cast<const TargetLayoutsField*>(field);
    if (!NX_ASSERT(targetLayoutField))
        return {QValidator::State::Invalid, Strings::invalidFieldType()};

    const auto targetLayoutFieldProperties = targetLayoutField->properties();
    const auto layoutIds = targetLayoutField->value();
    const bool isValidSelection =
        !layoutIds.empty() || targetLayoutFieldProperties.allowEmptySelection;

    if (!isValidSelection)
        return {QValidator::State::Invalid, tr("Select at least one layout")};

    const auto layouts =
        context->resourcePool()->getResourcesByIds<QnLayoutResource>(targetLayoutField->value());
    if (layouts.empty() && !layoutIds.empty())
        return {QValidator::State::Invalid, Strings::layoutsWereRemoved(layoutIds.size())};

    return {};
}

} // namespace nx::vms::rules
