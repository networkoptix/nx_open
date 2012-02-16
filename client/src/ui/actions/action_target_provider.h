#ifndef QN_ACTION_TARGET_PROVIDER_H
#define QN_ACTION_TARGET_PROVIDER_H

#include "action_fwd.h"

class QVariant;

class QnActionTargetProvider {
public:
    virtual ~QnActionTargetProvider() {};

    virtual QVariant target(QnAction *action) = 0;
};

#endif // QN_ACTION_TARGET_PROVIDER_H
