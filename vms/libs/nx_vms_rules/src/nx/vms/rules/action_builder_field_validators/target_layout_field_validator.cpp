// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_layout_field_validator.h"

#include <core/resource_management/resource_pool.h>
#include <nx/vms/rules/action_builder_fields/target_layout_field.h>

#include "../rule.h"
#include "../strings.h"

namespace nx::vms::rules {

ValidationResult TargetLayoutFieldValidator::validity(
    const Field* field, const Rule* /*rule*/, common::SystemContext* /*context*/) const
{
    auto targetLayoutField = dynamic_cast<const TargetLayoutField*>(field);
    if (!NX_ASSERT(targetLayoutField))
        return {QValidator::State::Invalid, Strings::invalidFieldType()};

    const auto targetLayoutFieldProperties = targetLayoutField->properties();
    const auto layoutIds = targetLayoutField->value();
    const bool isValidSelection =
        !layoutIds.empty() || targetLayoutFieldProperties.allowEmptySelection;

    if (!isValidSelection)
        return {QValidator::State::Invalid, tr("Select at least one layout")};

    return {};
}

} // namespace nx::vms::rules
