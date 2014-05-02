////////////////////////////////////////////////////////////
// 11 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef COMMON_BUSINESS_ACTION_H
#define COMMON_BUSINESS_ACTION_H

#include "abstract_business_action.h"

/*!
    Initiallly, this class has been created so that code like "new QnAbstractBusinessAction" never appears in project
*/
class QnCommonBusinessAction: public QnAbstractBusinessAction
{
    typedef QnAbstractBusinessAction base_type;
public:
    explicit QnCommonBusinessAction(const QnBusiness::ActionType actionType, const QnBusinessEventParameters &runtimeParams);
};

#endif  //COMMON_BUSINESS_ACTION_H
