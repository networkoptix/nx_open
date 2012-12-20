/**********************************************************
* 29 nov 2012
* a.kolesnikov
***********************************************************/

#include "sendmail_business_action.h"
#include "core/resource/resource.h"

namespace BusinessActionParameters {
    static QLatin1String emailAddress("emailAddress");

    QString getEmailAddress(const QnBusinessParams &params) {
        return params.value(emailAddress, QString()).toString();
    }

    void setEmailAddress(QnBusinessParams* params, const QString &value) {
        (*params)[emailAddress] = value;
    }

}

QnSendMailBusinessAction::QnSendMailBusinessAction(QnBusinessParams runtimeParams):
    base_type(BusinessActionType::BA_SendMail)
{
    m_eventType =           QnBusinessEventRuntime::getEventType(runtimeParams);
    m_eventResourceName =   QnBusinessEventRuntime::getEventResourceName(runtimeParams);
    m_eventResourceUrl =    QnBusinessEventRuntime::getEventResourceUrl(runtimeParams);
    m_eventDescription =    QnBusinessEventRuntime::getEventDescription(runtimeParams);
}

QString QnSendMailBusinessAction::getSubject() const {
    return !m_eventResourceName.isEmpty()
        ? QObject::tr("%1 Event received from resource %2 (%3)").
            arg(BusinessEventType::toString(m_eventType)).
            arg(m_eventResourceName).
            arg(m_eventResourceUrl)
        : QObject::tr("%1 Event received").
          arg(BusinessEventType::toString(m_eventType));
}

QString QnSendMailBusinessAction::getMessageBody() const {
    /*
        Event Input_Event caught from Axis (ip 192.168.12.13):
            timestamp: ...
            input port 1, state: on
        Parameters:
            param1 = value1
            param2 = value2
            ...
    */

    //TODO: modify to allow events without resources
    QString text;
    text += QObject::tr("Event %1 caught from %2 (address %3):\n").
        arg(BusinessEventType::toString(m_eventType)).
        arg(!m_eventResourceName.isEmpty() ? m_eventResourceName : QObject::tr("UNKNOWN")).
        arg(!m_eventResourceUrl.isEmpty() ? m_eventResourceUrl : QObject::tr("UNKNOWN"));
    text += m_eventDescription;

    text += QObject::tr("Action parameters:\n");
    //printing action params
    for( QnBusinessParams::const_iterator
        it = getParams().begin();
        it != getParams().end();
        ++it )
    {
        text += QLatin1String("  ") + it.key() + QLatin1String(" = ") + it.value().toString() + QLatin1String("\n");
    }

    return text;
}
