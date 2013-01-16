#ifndef BUSINESS_ACTION_FACTORY_H
#define BUSINESS_ACTION_FACTORY_H

#include "abstract_business_action.h"

class QnBusinessActionFactory
{
public:
    static QnAbstractBusinessActionPtr createAction(const BusinessActionType::Value actionType, const QnBusinessParams &runtimeParams);
};

#endif // BUSINESS_ACTION_FACTORY_H
