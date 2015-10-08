#include "debug_events_rest_handler.h"

#include <business/events/reasoned_business_event.h>
#include <business/events/network_issue_business_event.h>
#include <business/events/camera_disconnected_business_event.h>
#include <business/business_rule_processor.h>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <utils/network/http/httptypes.h>
#include <utils/common/synctime.h>

int QnDebugEventsRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) {
    QString action = extractAction(path);

    if (action == "networkIssue")
        return testNetworkIssue(params, result);

    if (action == "disconnected")
        return testCameraDisconnected(params, result);
    
    return nx_http::StatusCode::notFound;
}

int QnDebugEventsRestHandler::testNetworkIssue(const QnRequestParams & params, QnJsonRestResult &result) {

    QnVirtualCameraResourcePtr camera = qnResPool->getAllCameras(QnResourcePtr()).first();

    QString cameraId = params.value("cameraId");
    if (!cameraId.isEmpty())
        camera = qnResPool->getResourceByUniqueId<QnVirtualCameraResource>(cameraId);

    QString id = params.value("id");
    if (id == "noframe") {
        QnNetworkIssueBusinessEventPtr networkEvent(new QnNetworkIssueBusinessEvent(
            camera,
            qnSyncTime->currentUSecsSinceEpoch(), 
            QnBusiness::NetworkNoFrameReason, 
            QnNetworkIssueBusinessEvent::encodeTimeoutMsecs(5000)
            ));
        qnBusinessRuleProcessor->processBusinessEvent(networkEvent);
        return nx_http::StatusCode::ok;
    } else
    if (id == "closed") {
        QnNetworkIssueBusinessEventPtr networkEvent(new QnNetworkIssueBusinessEvent(
            camera,
            qnSyncTime->currentUSecsSinceEpoch(), 
            QnBusiness::NetworkConnectionClosedReason, 
            QnNetworkIssueBusinessEvent::encodePrimaryStream(true)
            ));
        qnBusinessRuleProcessor->processBusinessEvent(networkEvent);
        return nx_http::StatusCode::ok;
    } else
    if (id == "packetloss") {
        QnNetworkIssueBusinessEventPtr networkEvent(new QnNetworkIssueBusinessEvent(
            camera,
            qnSyncTime->currentUSecsSinceEpoch(), 
            QnBusiness::NetworkRtpPacketLossReason, 
            QnNetworkIssueBusinessEvent::encodePacketLossSequence(1234, 5678)
            ));
        qnBusinessRuleProcessor->processBusinessEvent(networkEvent);
        return nx_http::StatusCode::ok;
    }
  
    result.setError(QnRestResult::InvalidParameter, lit("Possible id values: noframe, closed, packetloss"));
    return nx_http::StatusCode::notImplemented;
}

int QnDebugEventsRestHandler::testCameraDisconnected(const QnRequestParams & params, QnJsonRestResult &result) {

    QnVirtualCameraResourcePtr camera = qnResPool->getAllCameras(QnResourcePtr()).first();

    QString cameraId = params.value("cameraId");
    if (!cameraId.isEmpty())
        camera = qnResPool->getResourceByUniqueId<QnVirtualCameraResource>(cameraId);

    QnAbstractBusinessEventPtr event(new QnCameraDisconnectedBusinessEvent(camera, qnSyncTime->currentMSecsSinceEpoch()));

    qnBusinessRuleProcessor->processBusinessEvent(event);
    return nx_http::StatusCode::ok;
}
