#pragma once

#include <ui/actions/action_fwd.h>
#include <ui/actions/actions.h>
#include <ui/actions/action_parameters.h>

class QnWorkbenchContext;

/**
 * Interface for querying current action scope from the application and
 * action target given an action scope.
 */
class QnActionTargetProvider
{
public:
    virtual ~QnActionTargetProvider() {}

    virtual Qn::ActionScope currentScope() const = 0;

    virtual QnActionParameters currentParameters(Qn::ActionScope scope) const = 0;
};
