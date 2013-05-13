#include "panic_business_action.h"
#include "core/resource/resource.h"

QnPanicBusinessAction::QnPanicBusinessAction(const QnBusinessParams &runtimeParams) :
    base_type(BusinessActionType::PanicRecording, runtimeParams)
{
}
