/**********************************************************
* 29 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef SENDMAILBUSINESSACTION_H
#define SENDMAILBUSINESSACTION_H

#include "abstract_business_action.h"
#include "abstract_business_event.h"

#include <core/resource/resource_fwd.h>

namespace BusinessActionParameters {
    QString getEmailAddress(const QnBusinessParams &params);
    void setEmailAddress(QnBusinessParams* params, const QString &value);
}

class QnSendMailBusinessAction: public QnAbstractBusinessAction
{
    typedef QnAbstractBusinessAction base_type;
public:
    QnSendMailBusinessAction( BusinessEventType::Value eventType, QnResourcePtr resource, QString eventDescription );

    QString getSubject() const;

    //!Convert action to human-readable string (for inserting into email body)
    QString getMessageBody() const;
private:
    BusinessEventType::Value m_eventType;
    QnResourcePtr m_eventResource;
    QString m_eventDescription;
};

typedef QSharedPointer<QnSendMailBusinessAction> QnSendMailBusinessActionPtr;

#endif  //SENDMAILBUSINESSACTION_H
