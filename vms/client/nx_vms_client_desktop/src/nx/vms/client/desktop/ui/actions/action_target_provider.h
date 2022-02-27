// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/ui/actions/action_types.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>

namespace nx::vms::client::desktop {
namespace ui {
namespace action {

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

} // namespace action
} // namespace ui
} // namespace nx::vms::client::desktop
