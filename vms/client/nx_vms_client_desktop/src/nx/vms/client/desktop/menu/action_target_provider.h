// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "action_parameters.h"
#include "action_types.h"

namespace nx::vms::client::desktop {
namespace menu {

/**
 * Interface for querying current action scope from the application and
 * action target given an action scope.
 */
class TargetProvider
{
public:
    virtual ~TargetProvider() {}
    virtual ActionScope currentScope() const = 0;
    virtual Parameters currentParameters(ActionScope scope) const = 0;
};

} // namespace menu
} // namespace nx::vms::client::desktop
