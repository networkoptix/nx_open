/**********************************************************
* 29 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef SENDMAILBUSINESSACTION_H
#define SENDMAILBUSINESSACTION_H

#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>

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
    QString resourceString(bool useUrl) const;
    QString timestampString() const;
    QString adresatesString() const;
    QString stateString() const;
    QString reasonString() const;
    QString conflictString() const;
};

typedef QSharedPointer<QnSendMailBusinessAction> QnSendMailBusinessActionPtr;

#endif  //SENDMAILBUSINESSACTION_H
