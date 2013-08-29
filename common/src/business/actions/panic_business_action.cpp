#include "panic_business_action.h"
#include "core/resource/resource.h"

QnPanicBusinessAction::QnPanicBusinessAction(const QnBusinessEventParameters &runtimeParams) :
    base_type(BusinessActionType::PanicRecording, runtimeParams)
{
}
