#include "event_log_rest_handler.h"

#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/events/abstract_event.h>

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

#include <nx/fusion/model_functions.h>

using namespace nx;

namespace {

static const QString kCameraIdParam = lit("cameraId");
static const QString kDeprecatedPhysicalIdParam = lit("physicalId");

} // namespace

int QnEventLogRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor* owner)
{
    QnTimePeriod period(-1, -1);
    QnSecurityCamResourceList resList;
    QString errStr;
    vms::event::EventType eventType = vms::event::undefinedEvent;
    QnUuid eventSubtype;
    vms::event::ActionType actionType = vms::event::undefinedAction;
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
                eventType = (vms::event::EventType) params[i].second.toInt();
                if (!vms::event::allEvents().contains(eventType)
                    && !vms::event::hasChild(eventType))
                {
                    errStr = QString("Invalid event type %1").arg(params[i].second);
                }
            }
            else if (params[i].first == "event_subtype")
            {
                eventSubtype = QnLexical::deserialized<QnUuid>(params[i].second);
            }
            else if (params[i].first == "action")
            {
                actionType = (vms::event::ActionType) params[i].second.toInt();
                if (!vms::event::allActions().contains(actionType))
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
        eventSubtype,
        actionType,
        businessRuleId);

    contentType = "application/octet-stream";
    return CODE_OK;
}

int QnEventLogRestHandler::executePost(
    const QString& path, const QnRequestParamList& params, const QByteArray& /*body*/,
    const QByteArray& /*srcBodyContentType*/, QByteArray& result, QByteArray& contentType,
    const QnRestConnectionProcessor* owner)
{
    return executeGet(path, params, result, contentType, owner);
}
