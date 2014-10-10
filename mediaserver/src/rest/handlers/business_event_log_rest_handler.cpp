#include "business_event_log_rest_handler.h"

#include "business/actions/abstract_business_action.h"

#include <core/resource/camera_resource.h>
#include "core/resource_management/resource_pool.h"

#include "events/events_db.h"

#include "utils/network/tcp_connection_priv.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"

#include <media_server/serverutil.h>

int QnBusinessEventLogRestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*)
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
            period.startTimeMs = params[i].second.toLongLong();
    }

    if (period.startTimeMs == -1)
    {
        errStr = "Parameter 'from' MUST be specified";
    }
    else {
        for (int i = 0; i < params.size(); ++i)
        {
            if (params[i].first == "to")
                period.durationMs = params[i].second.toLongLong() - period.startTimeMs;
            else if (params[i].first == "res_id") {
                QnResourcePtr res = qSharedPointerDynamicCast<QnVirtualCameraResource> (qnResPool->getNetResourceByPhysicalId(params[i].second));
                if (res)
                    resList << res;
                else
                    errStr = QString("Camera resource %1 not found").arg(params[i].second);
            }
            else if (params[i].first == "event") {
                eventType = (QnBusiness::EventType) params[i].second.toInt();
                if(!QnBusiness::allEvents().contains(eventType))
                    errStr = QString("Invalid event type %1").arg(params[i].second);
            }
            else if (params[i].first == "action") {
                actionType = (QnBusiness::ActionType) params[i].second.toInt();
                if (!QnBusiness::allActions().contains(actionType))
                    errStr = QString("Invalid action type %1.").arg(params[i].second);
            }
            else if (params[i].first == "brule_id") {
                businessRuleId = QnUuid(params[i].second);
            }
        }
    }

    if (!errStr.isEmpty())
    {
        result.append("<root>\n");
        result.append(errStr);
        result.append("</root>\n");
        return CODE_INVALID_PARAMETER;
    }

    qnEventsDB->getAndSerializeActions(
        result,
        period, 
        resList,
        eventType, 
        actionType,
        businessRuleId);

    contentType = "application/octet-stream";
    return CODE_OK;
}

int QnBusinessEventLogRestHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& /*body*/, const QByteArray& /*srcBodyContentType*/, QByteArray& result, 
                                               QByteArray& contentType, const QnRestConnectionProcessor* owner)
{
    return executeGet(path, params, result, contentType, owner);
}

