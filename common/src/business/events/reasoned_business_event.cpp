#include "reasoned_business_event.h"

QnReasonedBusinessEvent::QnReasonedBusinessEvent(const QnBusiness::EventType eventType,
                                                 const QnResourcePtr &resource,
                                                 const qint64 timeStamp,
                                                 const QnBusiness::EventReason reasonCode,
                                                 const QString &reasonParamsEncoded):
    base_type(eventType, resource, timeStamp),
    m_reasonCode(reasonCode),
    m_reasonParamsEncoded(reasonParamsEncoded)
{
}

QnBusinessEventParameters QnReasonedBusinessEvent::getRuntimeParams() const 
{

    QnBusinessEventParameters params = base_type::getRuntimeParams();
    params.setReasonCode(m_reasonCode);
    params.setReasonParamsEncoded(m_reasonParamsEncoded);
    return params;
}
