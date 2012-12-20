////////////////////////////////////////////////////////////
// 11 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef COMMON_BUSINESS_ACTION_H
#define COMMON_BUSINESS_ACTION_H

#include "abstract_business_action.h"


class QnCommonBusinessAction: public QnAbstractBusinessAction
{
    typedef QnAbstractBusinessAction base_type;
public:
    QnCommonBusinessAction( BusinessActionType::Value actionType );
};

#endif  //COMMON_BUSINESS_ACTION_H
