#include "panic_business_action.h"
#include "core/resource/resource.h"

QnPanicBusinessAction::QnPanicBusinessAction(const QnBusinessEventParameters &runtimeParams) :
    base_type(QnBusiness::PanicRecordingAction, runtimeParams)
{
}
