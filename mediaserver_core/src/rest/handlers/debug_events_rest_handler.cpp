#include "debug_events_rest_handler.h"

#include <nx/vms/event/events/events_fwd.h>
#include <nx/vms/event/events/reasoned_event.h>
#include <nx/vms/event/events/network_issue_event.h>
#include <nx/vms/event/events/camera_disconnected_event.h>
#include <nx/mediaserver/event/rule_processor.h>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/network/http/http_types.h>
#include <utils/common/synctime.h>
#include <rest/server/rest_connection_processor.h>

using namespace nx;

int QnDebugEventsRestHandler::executeGet(
    const QString &path,
    const QnRequestParams &params,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    QString action = extractAction(path);

    if (action == "networkIssue")
        return testNetworkIssue(params, result, owner);

    if (action == "disconnected")
        return testCameraDisconnected(params, result, owner);

    return nx::network::http::StatusCode::notFound;
}

int QnDebugEventsRestHandler::testNetworkIssue(
    const QnRequestParams & params,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    QnVirtualCameraResourcePtr camera = owner->resourcePool()->getAllCameras(QnResourcePtr()).first();

    QString cameraId = params.value("cameraId");
    if (!cameraId.isEmpty())
        camera = owner->resourcePool()->getResourceByUniqueId<QnVirtualCameraResource>(cameraId);

    QString param = params.value("param");

    QString id = params.value("id");
    if (id == "noframe")
    {
        if (param.isEmpty())
            param = vms::event::NetworkIssueEvent::encodeTimeoutMsecs(5000);

        vms::event::NetworkIssueEventPtr networkEvent(new vms::event::NetworkIssueEvent(
            camera,
            qnSyncTime->currentUSecsSinceEpoch(),
            vms::event::EventReason::networkNoFrame,
            param));

        qnEventRuleProcessor->processEvent(networkEvent);
        return nx::network::http::StatusCode::ok;
    }
    else if (id == "closed")
    {
        if (param.isEmpty())
            param = vms::event::NetworkIssueEvent::encodePrimaryStream(true);

        vms::event::NetworkIssueEventPtr networkEvent(new vms::event::NetworkIssueEvent(
            camera,
            qnSyncTime->currentUSecsSinceEpoch(),
            vms::event::EventReason::networkConnectionClosed,
            param));

        qnEventRuleProcessor->processEvent(networkEvent);
        return nx::network::http::StatusCode::ok;
    }
    else if (id == "packetloss")
    {
        if (param.isEmpty())
            param = vms::event::NetworkIssueEvent::encodePacketLossSequence(1234, 5678);

        vms::event::NetworkIssueEventPtr networkEvent(new vms::event::NetworkIssueEvent(
            camera,
            qnSyncTime->currentUSecsSinceEpoch(),
            vms::event::EventReason::networkRtpPacketLoss,
            param));

        qnEventRuleProcessor->processEvent(networkEvent);
        return nx::network::http::StatusCode::ok;
    }

    result.setError(QnRestResult::InvalidParameter, lit("Possible id values: noframe, closed, packetloss"));
    return nx::network::http::StatusCode::notImplemented;
}

int QnDebugEventsRestHandler::testCameraDisconnected(
    const QnRequestParams& params,
    QnJsonRestResult& /*result*/,
    const QnRestConnectionProcessor* owner)
{
    QnVirtualCameraResourcePtr camera = owner->resourcePool()->getAllCameras(QnResourcePtr()).first();

    QString cameraId = params.value("cameraId");
    if (!cameraId.isEmpty())
        camera = owner->resourcePool()->getResourceByUniqueId<QnVirtualCameraResource>(cameraId);

    vms::event::CameraDisconnectedEventPtr event(new vms::event::CameraDisconnectedEvent(
        camera, qnSyncTime->currentUSecsSinceEpoch()));

    qnEventRuleProcessor->processEvent(event);
    return nx::network::http::StatusCode::ok;
}
