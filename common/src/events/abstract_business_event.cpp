#include "abstract_business_event.h"
#include "utils/common/synctime.h"

QnAbstractBusinessEvent::QnAbstractBusinessEvent():
    m_eventType(BE_NotDefined),
    m_dateTime(qnSyncTime->currentUSecsSinceEpoch())
{

}
