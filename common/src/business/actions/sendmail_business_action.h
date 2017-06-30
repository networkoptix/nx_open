/**********************************************************
* 29 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef SENDMAILBUSINESSACTION_H
#define SENDMAILBUSINESSACTION_H

#include <business/actions/abstract_business_action.h>
#include <business/business_aggregation_info.h>

#include <core/resource/resource_fwd.h>

class QnBusinessAggregationInfo;

class QnSendMailBusinessAction: public QnAbstractBusinessAction
{
    typedef QnAbstractBusinessAction base_type;
public:
    explicit QnSendMailBusinessAction(const QnBusinessEventParameters &runtimeParams);
    ~QnSendMailBusinessAction() {}

    const QnBusinessAggregationInfo &aggregationInfo() const;
    void setAggregationInfo(const QnBusinessAggregationInfo &info);

    virtual void assign(const QnAbstractBusinessAction& other) override;

private:
    QnBusinessAggregationInfo m_aggregationInfo;
};

typedef QSharedPointer<QnSendMailBusinessAction> QnSendMailBusinessActionPtr;

#endif  //SENDMAILBUSINESSACTION_H
