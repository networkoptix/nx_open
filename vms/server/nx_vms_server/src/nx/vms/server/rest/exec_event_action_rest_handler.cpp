#include "exec_event_action_rest_handler.h"
#include <nx/vms/api/data/event_rule_data.h>
#include <nx/vms/server/event/event_message_bus.h>
#include <media_server/media_server_module.h>
#include <nx/vms/event/event_fwd.h>
#include <nx_ec/data/api_conversion_functions.h>

namespace nx::vms::server {

QnExecuteEventActionRestHandler::QnExecuteEventActionRestHandler(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
}

nx::network::rest::Response QnExecuteEventActionRestHandler::executePost(
    const nx::network::rest::Request& request)
{
    if (!request.content)
    {
        return nx::network::rest::Response::error(
            nx::network::rest::Result::BadRequest,
            "Missing request body.");
    }

    bool success = false;
    const auto action = QJson::deserialized<api::EventActionData>(
        request.content->body, api::EventActionData(), &success);
    if (!success)
    {
        return nx::network::rest::Response::error(
            nx::network::rest::Result::InvalidParameter,
            "Invalid json object.");
    }

    nx::vms::event::AbstractActionPtr businessAction;
    ec2::fromApiToResource(action, businessAction);
    businessAction->setReceivedFromRemoteHost(true);
    serverModule()->eventMessageBus()->at_actionReceived(businessAction);
    return nx::network::rest::Response(nx::network::http::StatusCode::ok);
}

} //namespace nx::vms::server
