/**********************************************************
* 29 nov 2012
* a.kolesnikov
***********************************************************/
#include "sendmail_business_action.h"

#include <core/resource/user_resource.h>
#include <utils/common/email.h>

QnSendMailBusinessAction::QnSendMailBusinessAction(const QnBusinessEventParameters &runtimeParams):
    base_type(BusinessActionType::SendMail, runtimeParams)
{
}

const QnBusinessAggregationInfo& QnSendMailBusinessAction::aggregationInfo() const {
    return m_aggregationInfo;
}

void QnSendMailBusinessAction::setAggregationInfo(const QnBusinessAggregationInfo &info) {
    m_aggregationInfo = info;
}

bool QnUserEmailAllowedPolicy::isResourceValid(const QnUserResourcePtr &resource) {
    return QnEmail::isValid(resource->getEmail());
}

QString QnUserEmailAllowedPolicy::getErrorText(int invalid, int total) {
    return tr("%1 of %2 selected users have invalid email.").arg(invalid).arg(total);
}
