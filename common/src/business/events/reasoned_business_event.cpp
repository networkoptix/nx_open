#include "reasoned_business_event.h"

QnReasonedBusinessEvent::QnReasonedBusinessEvent(const BusinessEventType::Value eventType,
                                                 const QnResourcePtr &resource,
                                                 const qint64 timeStamp,
                                                 const QnBusiness::EventReason reasonCode,
                                                 const QString &reasonText):
    base_type(eventType, resource, timeStamp),
    m_reasonCode(reasonCode),
    m_reasonText(reasonText)
{
}

QnBusinessEventParameters QnReasonedBusinessEvent::getRuntimeParams() const 
{

    QnBusinessEventParameters params = base_type::getRuntimeParams();
    params.setReasonCode(m_reasonCode);
    params.setReasonText(m_reasonText);
    return params;
}
