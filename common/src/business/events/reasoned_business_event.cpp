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
    QString paramKey = QString::number(m_reasonCode);
    if (m_reasonCode == QnBusiness::StorageIssueIoError || m_reasonCode == QnBusiness::StorageIssueNotEnoughSpeed)
        paramKey += QLatin1String("_") + m_reasonText;

    QnBusinessEventParameters params = base_type::getRuntimeParams();
    params.setReasonCode(m_reasonCode);
    params.setReasonText(m_reasonText);
    params.setParamsKey(paramKey);
    return params;
}
