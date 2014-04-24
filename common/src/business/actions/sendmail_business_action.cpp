/**********************************************************
* 29 nov 2012
* a.kolesnikov
***********************************************************/
#include "sendmail_business_action.h"

QnSendMailBusinessAction::QnSendMailBusinessAction(const QnBusinessEventParameters &runtimeParams):
    base_type(QnBusiness::SendMailAction, runtimeParams)
{
}

const QnBusinessAggregationInfo& QnSendMailBusinessAction::aggregationInfo() const {
    return m_aggregationInfo;
}

void QnSendMailBusinessAction::setAggregationInfo(const QnBusinessAggregationInfo &info) {
    m_aggregationInfo = info;
}
