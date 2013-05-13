////////////////////////////////////////////////////////////
// 11 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "common_business_action.h"

QnCommonBusinessAction::QnCommonBusinessAction(const BusinessActionType::Value actionType, const QnBusinessParams &runtimeParams ):
    base_type(actionType, runtimeParams)
{
}
