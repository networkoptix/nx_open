// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integration_action.h"

#include "../utils/field.h"

namespace nx::vms::rules {

const ItemDescriptor& IntegrationAction::manifest()
{
    NX_ASSERT(false, "This method should never be called.");
    static const ItemDescriptor kEmptyDescriptor;
    return kEmptyDescriptor;
}

} // namespace nx::vms::rules
