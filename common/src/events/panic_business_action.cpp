#include "panic_business_action.h"
#include "core/resource/resource.h"


QnPanicBusinessAction::QnPanicBusinessAction( BusinessEventType::Value eventType, QnResourcePtr resource, QString eventDescription ) :
    base_type(BusinessActionType::BA_PanicRecording)
{
}
