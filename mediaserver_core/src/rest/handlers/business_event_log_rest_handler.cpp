#include "business_event_log_rest_handler.h"

#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>

#include <core/resource/security_cam_resource.h>
#include "core/resource_management/resource_pool.h"

#include <database/server_db.h>

#include "network/tcp_connection_priv.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"

#include <media_server/serverutil.h>
#include "recording/time_period.h"
#include <api/helpers/camera_id_helper.h>
#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>

namespace {

static const QString kCameraIdParam = lit("cameraId");
static const QString kDeprecatedPhysicalIdParam = lit("physicalId");

} // namespace

int QnBusinessEventLogRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor* owner)
{
    QnTimePeriod period(-1, -1);
    QnSecurityCamResourceList resList;
    QString errStr;
    QnBusiness::EventType eventType = QnBusiness::UndefinedEvent;
    QnBusiness::ActionType actionType = QnBusiness::UndefinedAction;
    QnUuid businessRuleId;

    nx::camera_id_helper::findAllCamerasByFlexibleIds(
        owner->commonModule()->resourcePool(),
        &resList,
        params,
        {kCameraIdParam, kDeprecatedPhysicalIdParam});

    for (int i = 0; i < params.size(); ++i)
    {
        if (params[i].first == "from")
            period.startTimeMs = params[i].second.toLongLong();
    }

    if (period.startTimeMs == -1)
    {
        errStr = "Parameter 'from' MUST be specified";
    }
    else
    {
        for (int i = 0; i < params.size(); ++i)
        {
            if (params[i].first == "to")
            {
                period.durationMs = params[i].second.toLongLong() - period.startTimeMs;
            }
            else if (params[i].first == "event")
            {
                eventType = (QnBusiness::EventType) params[i].second.toInt();
                if (!QnBusiness::allEvents().contains(eventType)
                    && !QnBusiness::hasChild(eventType))
                {
                    errStr = QString("Invalid event type %1").arg(params[i].second);
                }
            }
            else if (params[i].first == "action")
            {
                actionType = (QnBusiness::ActionType) params[i].second.toInt();
                if (!QnBusiness::allActions().contains(actionType))
                    errStr = QString("Invalid action type %1.").arg(params[i].second);
            }
            else if (params[i].first == "brule_id")
            {
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

    qnServerDb->getAndSerializeActions(
        result,
        period,
        resList,
        eventType,
        actionType,
        businessRuleId);

    contentType = "application/octet-stream";
    return CODE_OK;
}

int QnBusinessEventLogRestHandler::executePost(
    const QString& path, const QnRequestParamList& params, const QByteArray& /*body*/,
    const QByteArray& /*srcBodyContentType*/, QByteArray& result, QByteArray& contentType,
    const QnRestConnectionProcessor* owner)
{
    return executeGet(path, params, result, contentType, owner);
}
