#include "ptzpreset_business_action.h"

QnPtzPresetBusinessAction::QnPtzPresetBusinessAction(const QnBusinessEventParameters &runtimeParams):
    base_type(QnBusiness::ExecutePtzPresetAction, runtimeParams)
{
}
