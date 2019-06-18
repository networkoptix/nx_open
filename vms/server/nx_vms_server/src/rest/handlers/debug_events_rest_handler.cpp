#include "debug_events_rest_handler.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>
#include <nx/network/http/http_types.h>
#include <nx/network/rest/nx_network_rest_ini.h>
#include <nx/utils/switch.h>
#include <nx/vms/event/events/camera_disconnected_event.h>
#include <nx/vms/event/events/events_fwd.h>
#include <nx/vms/event/events/network_issue_event.h>
#include <nx/vms/event/events/reasoned_event.h>
#include <nx/vms/server/event/extended_rule_processor.h>
#include <nx/vms/server/event/rule_processor.h>
#include <rest/server/rest_connection_processor.h>
#include <utils/common/synctime.h>

namespace nx::vms::server {

DebugEventsRestHandler::DebugEventsRestHandler(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
}

nx::network::rest::Response DebugEventsRestHandler::executeGet(
    const nx::network::rest::Request& request)
{
    if (!nx::network::rest::ini().allowModificationsViaGetMethod)
        return nx::network::rest::Response::error(nx::network::rest::Result::Forbidden);

    return executePost(request);
}

nx::network::rest::Response DebugEventsRestHandler::executePost(
    const nx::network::rest::Request& request)
{
    nx::network::rest::JsonResult result;
    const auto status = nx::utils::switch_(
        extractAction(request.path()),
        "networkIssue",
            [&] { return testNetworkIssue(request.params(), result, request.owner); },
        "disconnected",
            [&] { return testCameraDisconnected(request.params(), result, request.owner); },
        nx::utils::default_,
            [&] { return nx::network::http::StatusCode::notFound; });


    auto response = nx::network::rest::Response::result(result);
    response.statusCode = status;
    return response;
}

nx::network::http::StatusCode::Value DebugEventsRestHandler::testNetworkIssue(
    const nx::network::rest::Params& params,
    nx::network::rest::Result& result,
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
            vms::api::EventReason::networkNoFrame,
            param));

        serverModule()->eventRuleProcessor()->processEvent(networkEvent);
        return nx::network::http::StatusCode::ok;
    }
    else if (id == "closed")
    {
        if (param.isEmpty())
            param = vms::event::NetworkIssueEvent::encodePrimaryStream(true);

        vms::event::NetworkIssueEventPtr networkEvent(new vms::event::NetworkIssueEvent(
            camera,
            qnSyncTime->currentUSecsSinceEpoch(),
            vms::api::EventReason::networkConnectionClosed,
            param));

        serverModule()->eventRuleProcessor()->processEvent(networkEvent);
        return nx::network::http::StatusCode::ok;
    }
    else if (id == "packetloss")
    {
        vms::event::NetworkIssueEventPtr networkEvent(new vms::event::NetworkIssueEvent(
            camera,
            qnSyncTime->currentUSecsSinceEpoch(),
            vms::api::EventReason::networkRtpPacketLoss,
            param));

        serverModule()->eventRuleProcessor()->processEvent(networkEvent);
        return nx::network::http::StatusCode::ok;
    }

    result.setError(QnRestResult::InvalidParameter, lit("Possible id values: noframe, closed, packetloss"));
    return nx::network::http::StatusCode::notImplemented;
}

nx::network::http::StatusCode::Value DebugEventsRestHandler::testCameraDisconnected(
    const nx::network::rest::Params& params,
    nx::network::rest::Result& /*result*/,
    const QnRestConnectionProcessor* owner)
{
    QnVirtualCameraResourcePtr camera = owner->resourcePool()->getAllCameras(QnResourcePtr()).first();

    QString cameraId = params.value("cameraId");
    if (!cameraId.isEmpty())
        camera = owner->resourcePool()->getResourceByUniqueId<QnVirtualCameraResource>(cameraId);

    vms::event::CameraDisconnectedEventPtr event(new vms::event::CameraDisconnectedEvent(
        camera, qnSyncTime->currentUSecsSinceEpoch()));

    serverModule()->eventRuleProcessor()->processEvent(event);
    return nx::network::http::StatusCode::ok;
}

} // namespace nx::vms::server
