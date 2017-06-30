/**********************************************************
* 29 nov 2012
* a.kolesnikov
***********************************************************/
#include "sendmail_business_action.h"

#include <nx/utils/log/log.h>

QnSendMailBusinessAction::QnSendMailBusinessAction(const QnBusinessEventParameters& runtimeParams):
    base_type(QnBusiness::SendMailAction, runtimeParams)
{
}

const QnBusinessAggregationInfo& QnSendMailBusinessAction::aggregationInfo() const
{
    return m_aggregationInfo;
}

void QnSendMailBusinessAction::setAggregationInfo(const QnBusinessAggregationInfo& info)
{
    m_aggregationInfo = info;
}

void QnSendMailBusinessAction::assign(const QnAbstractBusinessAction& other)
{
    base_type::assign(other);

    if (other.actionType() != QnBusiness::SendMailAction)
        return;

    const auto otherSendMail = dynamic_cast<const QnSendMailBusinessAction*>(&other);
    NX_ASSERT(otherSendMail);
    if (!otherSendMail)
        return;

    m_aggregationInfo = otherSendMail->m_aggregationInfo;
}
