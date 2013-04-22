////////////////////////////////////////////////////////////
// 11 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef COMMON_BUSINESS_ACTION_H
#define COMMON_BUSINESS_ACTION_H

#include "abstract_business_action.h"

// TODO: #AK what is the purpose of this class?
class QnCommonBusinessAction: public QnAbstractBusinessAction
{
    typedef QnAbstractBusinessAction base_type;
public:
    explicit QnCommonBusinessAction(const BusinessActionType::Value actionType, const QnBusinessParams &runtimeParams);
};

#endif  //COMMON_BUSINESS_ACTION_H
