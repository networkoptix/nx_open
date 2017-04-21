#pragma once

#include <nx/client/desktop/ui/actions/action_types.h>
#include <ui/actions/action_parameters.h>

/**
 * Interface for querying current action scope from the application and
 * action target given an action scope.
 */
class QnActionTargetProvider
{
public:
    virtual ~QnActionTargetProvider() {}
    virtual nx::client::desktop::ui::action::ActionScope currentScope() const = 0;
    virtual QnActionParameters currentParameters(nx::client::desktop::ui::action::ActionScope scope) const = 0;
};
