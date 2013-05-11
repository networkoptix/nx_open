/**********************************************************
* 29 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef SENDMAILBUSINESSACTION_H
#define SENDMAILBUSINESSACTION_H

#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>
#include <business/business_aggregation_info.h>

class QnBusinessAggregationInfo;

class QnSendMailBusinessAction: public QnAbstractBusinessAction
{
    typedef QnAbstractBusinessAction base_type;
public:
    explicit QnSendMailBusinessAction(const QnBusinessParams &runtimeParams);
    ~QnSendMailBusinessAction() {}

    QString getSubject() const;

    //!Convert action to human-readable string (for inserting into email body)
    QString getMessageBody() const;

    void setAggregationInfo(const QnBusinessAggregationInfo &info);
private:
    QnBusinessAggregationInfo m_aggregationInfo;
};

typedef QSharedPointer<QnSendMailBusinessAction> QnSendMailBusinessActionPtr;

#endif  //SENDMAILBUSINESSACTION_H
