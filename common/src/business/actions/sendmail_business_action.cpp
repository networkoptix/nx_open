/**********************************************************
* 29 nov 2012
* a.kolesnikov
***********************************************************/

#include "sendmail_business_action.h"

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_managment/resource_pool.h>

namespace BusinessActionParameters {
    static QLatin1String emailAddress("emailAddress");

    QString getEmailAddress(const QnBusinessParams &params) {
        return params.value(emailAddress, QString()).toString();
    }

    void setEmailAddress(QnBusinessParams* params, const QString &value) {
        (*params)[emailAddress] = value;
    }

}

QnSendMailBusinessAction::QnSendMailBusinessAction(const QnBusinessParams &runtimeParams):
    base_type(BusinessActionType::BA_SendMail, runtimeParams)
{
    int id = QnBusinessEventRuntime::getEventResourceId(runtimeParams);
    QnResourcePtr res = id > 0 ? qnResPool->getResourceById(id, QnResourcePool::rfAllResources) : QnResourcePtr();

    m_eventType =           QnBusinessEventRuntime::getEventType(runtimeParams);
    m_eventResourceName =   res ? res->getName() : QString();
    m_eventResourceUrl =    res ? res->getUrl() : QString();
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
    if (getAggregationCount() > 1)
        text += QString(QLatin1String("  (repeated %1 times)")).arg(getAggregationCount());

    text += QObject::tr("Adresates:\n");
    QnResourceList resources = getResources();
    foreach (const QnUserResourcePtr &user, resources.filtered<QnUserResource>())
        text += user->getName() + QLatin1Char('\n');

    QString additional = BusinessActionParameters::getEmailAddress(getParams());
    QStringList receivers = additional.split(QLatin1Char(';'), QString::SkipEmptyParts);
    foreach (const QString &receiver, receivers)
        text += receiver.trimmed() + QLatin1Char('\n');

    return text;
}
