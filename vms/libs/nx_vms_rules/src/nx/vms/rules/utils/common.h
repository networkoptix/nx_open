// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>

#include "../rules_fwd.h"
#include "field.h"

namespace nx::vms::rules::utils {

inline bool aggregateByType(
    const ItemDescriptor& eventDescriptor,
    const ItemDescriptor& actionDescriptor)
{
    return actionDescriptor.flags.testFlag(ItemFlag::aggregationByTypeSupported)
        && eventDescriptor.flags.testFlag(ItemFlag::aggregationByTypeSupported);
}

/** Returns whether logging is allowed for the given rule. */
NX_VMS_RULES_API bool isLoggingAllowed(const Engine* engine, QnUuid ruleId);

} // namespace nx::vms::rules
