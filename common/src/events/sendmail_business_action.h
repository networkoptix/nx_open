/**********************************************************
* 29 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef SENDMAILBUSINESSACTION_H
#define SENDMAILBUSINESSACTION_H

#include "abstract_business_action.h"
#include "abstract_business_event.h"

namespace BusinessActionParameters {
    QString getEmailAddress(const QnBusinessParams &params);
    void setEmailAddress(QnBusinessParams* params, const QString &value);
}

class QnSendMailBusinessAction: public QnAbstractBusinessAction
{
    typedef QnAbstractBusinessAction base_type;
public:
    explicit QnSendMailBusinessAction(const QnBusinessParams &runtimeParams);
    ~QnSendMailBusinessAction() {}

    QString getSubject() const;

    //!Convert action to human-readable string (for inserting into email body)
    QString getMessageBody() const;
private:
    BusinessEventType::Value m_eventType;
    QString m_eventResourceName;
    QString m_eventResourceUrl;
    QString m_eventDescription;
};

typedef QSharedPointer<QnSendMailBusinessAction> QnSendMailBusinessActionPtr;

#endif  //SENDMAILBUSINESSACTION_H
