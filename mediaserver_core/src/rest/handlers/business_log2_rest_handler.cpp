#include "business_log2_rest_handler.h"

#include <common/common_module.h>
#include "events/events_db.h"
#include "recording/time_period.h"
#include "rest/server/json_rest_result.h"
#include "business/business_event_parameters.h"
#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>

#include <core/resource/camera_resource.h>
#include "core/resource_management/resource_pool.h"
#include "utils/common/string.h"

int QnBusinessLog2RestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& contentBody, QByteArray& contentType, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path)
    Q_UNUSED(params)
    Q_UNUSED(contentType)

    QnTimePeriod period(-1,-1);
    QnResourceList resList;
    QString errStr;
    QnBusiness::EventType eventType = QnBusiness::UndefinedEvent;
    QnBusiness::ActionType actionType = QnBusiness::UndefinedAction;
    QnUuid businessRuleId;

    for (int i = 0; i < params.size(); ++i)
    {
        if (params[i].first == "from")
            period.startTimeMs = parseDateTime(params[i].second) / 1000;
        else if (params[i].first == "to")
            period.durationMs = parseDateTime(params[i].second)/1000 - period.startTimeMs;
        else if (params[i].first == "physicalId") {
            QnResourcePtr res = qSharedPointerDynamicCast<QnVirtualCameraResource> (qnResPool->getNetResourceByPhysicalId(params[i].second));
            if (res)
                resList << res;
            else
                errStr = QString("Camera resource %1 not found").arg(params[i].second);
        }
        else if (params[i].first == "event_type") {
            eventType = QnLexical::deserialized<QnBusiness::EventType>(params[i].second);
            if(!QnBusiness::allEvents().contains(eventType) 
                && !QnBusiness::hasChild(eventType))
                errStr = QString("Invalid event type %1").arg(params[i].second);
        }
        else if (params[i].first == "action_type") {
            actionType = QnLexical::deserialized<QnBusiness::ActionType>(params[i].second);
            if (!QnBusiness::allActions().contains(actionType))
                errStr = QString("Invalid action type %1.").arg(params[i].second);
        }
        else if (params[i].first == "brule_id") {
            businessRuleId = QnUuid(params[i].second);
        }
    }
    if (period.startTimeMs == -1)
        errStr = "Parameter 'from' MUST be specified";

    QnRestResult restResult;
    QnBusinessActionDataList outputData;
    if (errStr.isEmpty())
    {
        outputData =
            qnEventsDB->getActions(
                period, 
                resList,
                eventType, 
                actionType,
                businessRuleId);

    }
    else
        restResult.setError(QnRestResult::InvalidParameter, errStr);

    QnFusionRestHandlerDetail::serializeRestReply(outputData, params, contentBody, contentType, restResult);


    return nx_http::StatusCode::ok;
}
