#include "mserver_failure_business_event.h"
#include "core/resource/resource.h"

QnMServerFailureBusinessEvent::QnMServerFailureBusinessEvent(
        const QnResourcePtr& resource,
        qint64 timeStamp,
        int reasonCode):
    base_type(BusinessEventType::BE_MediaServer_Failure,
                            resource,
                            timeStamp,
                            reasonCode)
{
}

QString QnMServerFailureBusinessEvent::toString() const
{
    QString reasonText;
    switch (m_reasonCode) {
        case QnBusiness::MServerIssueTerminated:
            reasonText = QObject::tr("Media server was terminated unexpectedly");
            break;
        case QnBusiness::MServerIssueStarted:
            reasonText = QObject::tr("Media server is started after an unexpected shutdown");
            break;
        default:
            break;
    }


    QString text = QnAbstractBusinessEvent::toString();
    text += QObject::tr(". Reason: %1.\n").arg(reasonText);
    return text;
}
