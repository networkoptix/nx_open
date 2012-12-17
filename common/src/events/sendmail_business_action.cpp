/**********************************************************
* 29 nov 2012
* a.kolesnikov
***********************************************************/

#include "abstract_business_event.h"
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

QnSendMailBusinessAction::QnSendMailBusinessAction( BusinessEventType::Value eventType, QnResourcePtr resource, QString eventDescription ) :
    base_type(BusinessActionType::BA_SendMail),
    m_eventType(eventType),
    m_eventResource(resource),
    m_eventDescription(eventDescription)
{
}

QString QnSendMailBusinessAction::getSubject() const {
    return m_eventResource
        ? QObject::tr("%1 Event received from resource %2 (%3)").
            arg(BusinessEventType::toString(m_eventType)).
            arg(m_eventResource->getName()).
            arg(m_eventResource->getUrl())
        : QObject::tr("%1 Event received from an unknown resource").
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

    QString text;
    text += QObject::tr("Event %1 caught from %2 (address %3):\n").
        arg(BusinessEventType::toString(m_eventType)).
        arg(m_eventResource ? m_eventResource->getName() : QObject::tr("UNKNOWN")).
        arg(m_eventResource ? m_eventResource->getUrl() : QObject::tr("UNKNOWN"));
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
