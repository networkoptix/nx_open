#include "reasoned_business_event.h"

namespace QnBusinessEventRuntime {

    static QLatin1String reasonCodeStr("reasonCode");
    static QLatin1String reasonTextStr("reasonText");

    int getReasonCode(const QnBusinessParams &params) {
        return params[reasonCodeStr].toInt();
    }

    void setReasonCode(QnBusinessParams* params, int value) {
        (*params)[reasonCodeStr] = value;
    }

    QString getReasonText(const QnBusinessParams &params) {
        return params[reasonTextStr].toString();
    }

    void setReasonText(QnBusinessParams* params, QString value) {
        (*params)[reasonTextStr] = value;
    }
}

QnReasonedBusinessEvent::QnReasonedBusinessEvent(const BusinessEventType::Value eventType,
                                                 const QnResourcePtr &resource,
                                                 const qint64 timeStamp,
                                                 const int reasonCode,
                                                 const QString &reasonText):
    base_type(eventType, resource, timeStamp),
    m_reasonCode(reasonCode),
    m_reasonText(reasonText)
{
}

QnBusinessParams QnReasonedBusinessEvent::getRuntimeParams() const {
    QnBusinessParams params = base_type::getRuntimeParams();
    QnBusinessEventRuntime::setReasonCode(&params, m_reasonCode);
    QnBusinessEventRuntime::setReasonText(&params, m_reasonText);
    return params;
}
