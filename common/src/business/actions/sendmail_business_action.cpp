/**********************************************************
* 29 nov 2012
* a.kolesnikov
***********************************************************/

#include "sendmail_business_action.h"

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <business/events/reasoned_business_event.h>
#include <business/events/conflict_business_event.h>
#include <business/events/camera_input_business_event.h>

#include "version.h"
#include "api/app_server_connection.h"
#include "business/business_strings_helper.h"

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

