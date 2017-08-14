#include "event_log2_rest_handler.h"

#include <api/helpers/camera_id_helper.h>
#include <common/common_module.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource_management/resource_pool.h>
#include <database/server_db.h>
#include <recording/time_period.h>
#include <rest/server/json_rest_result.h>
#include <rest/server/rest_connection_processor.h>
#include <nx/utils/string.h>
#include <nx/vms/event/event_parameters.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/events/abstract_event.h>

using namespace nx;

namespace {

static const QString kCameraIdParam = lit("cameraId");
static const QString kDeprecatedPhysicalIdParam = lit("physicalId");

} // namespace

int QnEventLog2RestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& params,
    QByteArray& contentBody,
    QByteArray& contentType,
    const QnRestConnectionProcessor* owner)
{
    QnTimePeriod period(-1, -1);
    QnSecurityCamResourceList resList;
    QString errStr;
    vms::event::EventType eventType = vms::event::undefinedEvent;
    QnUuid eventSubtype;
    vms::event::ActionType actionType = vms::event::undefinedAction;
    QnUuid ruleId;

    nx::camera_id_helper::findAllCamerasByFlexibleIds(
        owner->resourcePool(),
        &resList,
        params,
        {kCameraIdParam, kDeprecatedPhysicalIdParam});

    for (int i = 0; i < params.size(); ++i)
    {
        if (params[i].first == "from")
        {
            period.startTimeMs = nx::utils::parseDateTime(params[i].second) / 1000;
        }
        else if (params[i].first == "to")
        {
            period.durationMs =
                nx::utils::parseDateTime(params[i].second) / 1000 - period.startTimeMs;
        }
        else if (params[i].first == "event_type")
        {
            eventType = QnLexical::deserialized<vms::event::EventType>(params[i].second);
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
        else if (params[i].first == "action_type")
        {
            actionType = QnLexical::deserialized<vms::event::ActionType>(params[i].second);
            if (!vms::event::allActions().contains(actionType))
                errStr = QString("Invalid action type %1.").arg(params[i].second);
        }
        else if (params[i].first == "brule_id")
        {
            ruleId = QnUuid(params[i].second);
        }
    }
    if (period.startTimeMs == -1)
        errStr = "Parameter 'from' MUST be specified";

    QnRestResult restResult;
    vms::event::ActionDataList outputData;
    if (errStr.isEmpty())
    {
        outputData = qnServerDb->getActions(period,
            resList,
            eventType,
            eventSubtype,
            actionType,
            ruleId);
    }
    else
    {
        restResult.setError(QnRestResult::InvalidParameter, errStr);
    }

    QnFusionRestHandlerDetail::serializeRestReply(
        outputData, params, contentBody, contentType, restResult);

    return nx_http::StatusCode::ok;
}
