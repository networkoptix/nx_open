#pragma once

#include <nx/client/desktop/ui/actions/action_types.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>

namespace nx {
namespace client {
namespace desktop {
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
} // namespace desktop
} // namespace client
} // namespace nx