/**********************************************************
* 29 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef SENDMAILBUSINESSACTION_H
#define SENDMAILBUSINESSACTION_H

#include "abstract_business_action.h"
#include "abstract_business_event.h"


class QnSendMailBusinessAction
:
    public QnAbstractBusinessAction
{
public:
    QnSendMailBusinessAction( QnAbstractBusinessEventPtr eventPtr );

    //!Convert action to printable string
    virtual QString toString() const;

    QString emailAddress() const;
    void setEmailAddress( const QString& newEmailAddress );

    QnAbstractBusinessEventPtr getEvent() const;

private:
    QnAbstractBusinessEventPtr m_eventPtr;
};

typedef QSharedPointer<QnSendMailBusinessAction> QnSendMailBusinessActionPtr;

#endif  //SENDMAILBUSINESSACTION_H
