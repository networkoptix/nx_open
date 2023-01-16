// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action.h"

#include "../action_builder.h"
#include "../action_builder_fields/optional_time_field.h"
#include "../engine.h"
#include "../manifest.h"
#include "field.h"

namespace nx::vms::rules {

bool isProlonged(const Engine* engine, const ActionBuilder* builder)
{
    const auto manifest = engine->actionDescriptor(builder->actionType());
    if (!manifest || !manifest->flags.testFlag(ItemFlag::prolonged))
        return false;

    const auto durationField = builder->fieldByName<OptionalTimeField>(utils::kDurationFieldName);
    return (!durationField || durationField->value() <= std::chrono::seconds::zero());
}

} // namespace nx::vms::rules
