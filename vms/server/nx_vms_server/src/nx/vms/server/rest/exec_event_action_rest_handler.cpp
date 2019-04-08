#include "exec_event_action_rest_handler.h"
#include <nx/vms/api/data/event_rule_data.h>
#include <nx/vms/server/event/event_message_bus.h>
#include <media_server/media_server_module.h>
#include <nx/vms/event/event_fwd.h>
#include <nx_ec/data/api_conversion_functions.h>

namespace nx::vms::server::rest {

QnExecuteEventActionRestHandler::QnExecuteEventActionRestHandler(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
}

JsonRestResponse QnExecuteEventActionRestHandler::executePost(
    const JsonRestRequest& request, const QByteArray& body)
{
    JsonRestResponse response(nx::network::http::StatusCode::ok);

    bool success = false;
    const auto action = QJson::deserialized<api::EventActionData>(body, api::EventActionData(), &success);
    if (!success)
    {
        response.json.setError(
            QnRestResult::InvalidParameter, "Invalid json object.");
        return response;
    }

    nx::vms::event::AbstractActionPtr businessAction;
    ec2::fromApiToResource(action, businessAction);
    businessAction->setReceivedFromRemoteHost(true);
    serverModule()->eventMessageBus()->at_actionReceived(businessAction);
    return response;
}

} //namespace nx::vms::server::rest
