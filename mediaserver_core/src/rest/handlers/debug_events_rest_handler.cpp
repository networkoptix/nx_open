#include "debug_events_rest_handler.h"

#include <business/events/reasoned_business_event.h>
#include <business/events/network_issue_business_event.h>
#include <business/events/camera_disconnected_business_event.h>
#include <business/business_rule_processor.h>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/network/http/http_types.h>
#include <utils/common/synctime.h>
#include <rest/server/rest_connection_processor.h>

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

    return nx_http::StatusCode::notFound;
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
    if (id == "noframe") {
        if (param.isEmpty())
            param = QnNetworkIssueBusinessEvent::encodeTimeoutMsecs(5000);

        QnNetworkIssueBusinessEventPtr networkEvent(new QnNetworkIssueBusinessEvent(
            camera,
            qnSyncTime->currentUSecsSinceEpoch(),
            QnBusiness::NetworkNoFrameReason,
            param
            ));
        qnBusinessRuleProcessor->processBusinessEvent(networkEvent);
        return nx_http::StatusCode::ok;
    } else
    if (id == "closed") {
        if (param.isEmpty())
            param = QnNetworkIssueBusinessEvent::encodePrimaryStream(true);

        QnNetworkIssueBusinessEventPtr networkEvent(new QnNetworkIssueBusinessEvent(
            camera,
            qnSyncTime->currentUSecsSinceEpoch(),
            QnBusiness::NetworkConnectionClosedReason,
            param
            ));
        qnBusinessRuleProcessor->processBusinessEvent(networkEvent);
        return nx_http::StatusCode::ok;
    } else
    if (id == "packetloss") {
        if (param.isEmpty())
            param = QnNetworkIssueBusinessEvent::encodePacketLossSequence(1234, 5678);

        QnNetworkIssueBusinessEventPtr networkEvent(new QnNetworkIssueBusinessEvent(
            camera,
            qnSyncTime->currentUSecsSinceEpoch(),
            QnBusiness::NetworkRtpPacketLossReason,
            param
            ));
        qnBusinessRuleProcessor->processBusinessEvent(networkEvent);
        return nx_http::StatusCode::ok;
    }

    result.setError(QnRestResult::InvalidParameter, lit("Possible id values: noframe, closed, packetloss"));
    return nx_http::StatusCode::notImplemented;
}

int QnDebugEventsRestHandler::testCameraDisconnected(
    const QnRequestParams & params,
    QnJsonRestResult &/*result*/,
    const QnRestConnectionProcessor* owner)
{

    QnVirtualCameraResourcePtr camera = owner->resourcePool()->getAllCameras(QnResourcePtr()).first();

    QString cameraId = params.value("cameraId");
    if (!cameraId.isEmpty())
        camera = owner->resourcePool()->getResourceByUniqueId<QnVirtualCameraResource>(cameraId);

    QnAbstractBusinessEventPtr event(new QnCameraDisconnectedBusinessEvent(camera, qnSyncTime->currentUSecsSinceEpoch()));

    qnBusinessRuleProcessor->processBusinessEvent(event);
    return nx_http::StatusCode::ok;
}
