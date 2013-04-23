#ifndef QN_ACTION_TARGET_PROVIDER_H
#define QN_ACTION_TARGET_PROVIDER_H

#include "action_fwd.h"
#include "actions.h"
#include "action_parameters.h"

class QVariant;
class QnWorkbenchContext;

/**
 * Interface for querying current action scope from the application and 
 * action target given an action scope.
 */
class QnActionTargetProvider {
public:
    virtual ~QnActionTargetProvider() {}

    virtual Qn::ActionScope currentScope() const = 0;

    virtual QnActionParameters currentParameters(Qn::ActionScope scope) const {
        return QnActionParameters(currentTarget(scope));
    }

protected:
    virtual QVariant currentTarget(Qn::ActionScope scope) const = 0;
};

#endif // QN_ACTION_TARGET_PROVIDER_H
