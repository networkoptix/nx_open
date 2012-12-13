/**********************************************************
* 29 nov 2012
* a.kolesnikov
***********************************************************/

#include "abstract_business_event.h"
#include "sendmail_business_action.h"
#include "core/resource/resource.h"


QnSendMailBusinessAction::QnSendMailBusinessAction( QnAbstractBusinessEventPtr eventPtr ) :
    base_type(BusinessActionType::BA_SendMail),
    m_eventPtr( eventPtr )
{
}

QString QnSendMailBusinessAction::toString() const
{
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
    if( m_eventPtr )
    {
        const QnResourcePtr& eventRes = m_eventPtr->getResource();
        text += QString::fromLatin1("Event %1 caught from %2 (address %3):\n").
            arg(BusinessEventType::toString(m_eventPtr->getEventType())).arg(eventRes ? eventRes->getName() : QLatin1String("UNKNOWN")).
            arg(eventRes ? eventRes->getUrl() : QLatin1String("UNKNOWN"));
        text += m_eventPtr->toString();
    }

    text += QLatin1String("Action parameters:\n");
    //printing action params
    for( QnBusinessParams::const_iterator
        it = m_params.begin();
        it != m_params.end();
        ++it )
    {
        text += QLatin1String("  ") + it.key() + QLatin1String(" = ") + it.value().toString() + QLatin1String("\n");
    }

    return text;
}

QString QnSendMailBusinessAction::emailAddress() const
{
    return m_params.value( BusinessActionParamName::emailAddress ).toString();
}

void QnSendMailBusinessAction::setEmailAddress( const QString& newEmailAddress )
{
    m_params[BusinessActionParamName::emailAddress] = newEmailAddress;
}

QnAbstractBusinessEventPtr QnSendMailBusinessAction::getEvent() const
{
    return m_eventPtr;
}
