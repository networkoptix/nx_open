/**********************************************************
* 29 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef SENDMAILBUSINESSACTION_H
#define SENDMAILBUSINESSACTION_H

#include <QtCore/QCoreApplication> //for Q_DECLARE_TR_FUNCTIONS

#include <business/business_resource_validator.h>
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
private:
    QnBusinessAggregationInfo m_aggregationInfo;
};

typedef QSharedPointer<QnSendMailBusinessAction> QnSendMailBusinessActionPtr;

class QnUserEmailAllowedPolicy {
    Q_DECLARE_TR_FUNCTIONS(QnUserEmailAllowedPolicy)
public:
    typedef QnUserResource resource_type;
    static inline bool emptyListIsValid() { return false; }
    static bool isResourceValid(const QnUserResourcePtr &user);
    static QString getErrorText(int invalid, int total);
};

typedef QnBusinessResourceValidator<QnUserEmailAllowedPolicy> QnUserEmailValidator;

#endif  //SENDMAILBUSINESSACTION_H
