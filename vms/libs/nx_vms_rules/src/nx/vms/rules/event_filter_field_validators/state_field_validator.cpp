// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "state_field_validator.h"

#include "../action_builder.h"
#include "../event_filter.h"
#include "../event_filter_fields/state_field.h"
#include "../rule.h"
#include "../strings.h"
#include "../utils/common.h"

namespace nx::vms::rules {

ValidationResult StateFieldValidator::validity(
    const Field* field, const Rule* rule, common::SystemContext* context) const
{
    const auto stateField = dynamic_cast<const StateField*>(field);
    if (!NX_ASSERT(stateField))
        return {QValidator::State::Invalid, Strings::invalidFieldType()};

    const auto availableStates = utils::getPossibleFilterStates(context->vmsRulesEngine(), rule);
    if (!availableStates.contains(stateField->value()))
    {
        return {
            QValidator::State::Invalid,
            tr("`%1` state is not valid for the `%2` event and `%3` action with the given parameters")
               .arg(reflect::toString(stateField->value()).c_str())
               .arg(rule->eventFilters().first()->eventType())
               .arg(rule->actionBuilders().first()->actionType())
        };
    }

    return {};
}

} // namespace nx::vms::rules
