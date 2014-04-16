#include "business_event_log_rest_handler.h"

#include "business/actions/abstract_business_action.h"

#include <core/resource/camera_resource.h>
#include "core/resource_management/resource_pool.h"

#include "events/events_db.h"

#include "utils/network/tcp_connection_priv.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"

#include <media_server/serverutil.h>

int QnBusinessEventLogRestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(path)
    Q_UNUSED(params)
    Q_UNUSED(contentType)

    QnTimePeriod period(-1,-1);
    QnResourceList resList;
    QString errStr;
    BusinessEventType::Value  eventType = BusinessEventType::NotDefined;
    BusinessActionType::Value  actionType = BusinessActionType::NotDefined;
    QnId businessRuleId;

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
                eventType = (BusinessEventType::Value) params[i].second.toInt();
                if (eventType < 0)
                    errStr = QString("Invalid event type %1. Valid range is [0..%2]").arg(params[i].second).arg(BusinessEventType::NotDefined-1);
            }
            else if (params[i].first == "action") {
                actionType = (BusinessActionType::Value) params[i].second.toInt();
                if (actionType < 0 || actionType >= BusinessActionType::Count)
                    errStr = QString("Invalid action type %1. Valid range is [0..%2]").arg(params[i].second).arg(BusinessActionType::NotDefined-1);
            }
            else if (params[i].first == "brule_id") {
                businessRuleId = params[i].second;
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

int QnBusinessEventLogRestHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(body)
    return executeGet(path, params, result, contentType);
}

QString QnBusinessEventLogRestHandler::description() const
{
    return 
        "Returns event log"
        "<BR>Param <b>from</b> - start of time period at ms since 1.1.1970 (UTC format)"
        "<BR>Param <b>to</b> - end of time period at ms since 1.1.1970 (UTC format). Optional"
        "<BR>Param <b>format</b> - allowed values: <b>text</b>, <b>protobuf. Optional</b>"
        "<BR>Param <b>event</b> - event type. Optional</b>"
        "<BR>Param <b>action</b> - action type. Optional</b>"
        "<BR>Param <b>brule_id</b> - business rule id. Optional</b>";
}
