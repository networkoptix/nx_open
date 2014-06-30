////////////////////////////////////////////////////////////
// 11 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "common_business_action.h"

QnCommonBusinessAction::QnCommonBusinessAction(const QnBusiness::ActionType actionType, const QnBusinessEventParameters &runtimeParams ):
    base_type(actionType, runtimeParams)
{
}
