#include "event_message_bus.h"

#include <QtCore/QUrlQuery>

#include <api/app_server_connection.h>
#include <common/common_module.h>

#include <nx_ec/ec_api.h>
#include <nx/vms/api/data/event_rule_data.h>
#include <nx_ec/data/api_conversion_functions.h>

#include <nx/fusion/serialization/binary.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <api/server_rest_connection.h>

namespace nx {
namespace vms::server {
namespace event {

EventMessageBus::EventMessageBus(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

EventMessageBus::~EventMessageBus()
{
    const int kStopTimeoutMs = 5000;

    quit();
    if (!wait(kStopTimeoutMs))
    {
        terminate();
        wait();
    }
}

int EventMessageBus::deliverAction(const vms::event::AbstractActionPtr& action, const QnUuid& dstPeer)
{
    nx::vms::api::EventActionData actionData;
    ec2::fromResourceToApi(action, actionData);

    auto server = commonModule()->resourcePool()->getResourceById<QnMediaServerResource>(dstPeer);
    if (!server)
    {
        NX_WARNING(this, "Can't delivery event to the  target server %1. Not found", dstPeer);
        return 0;
    }
    auto connection = server->restConnection();
    connection->executeEventAction(actionData,
        [this, action, dstPeer](bool success, rest::Handle requestId, QnJsonRestResult result)
        {
            if (success)
            {
                emit actionDelivered(action);
            }
            else
            {
                NX_WARNING(this, "Delivery business action to the target server %1 is failed. Error: %2", dstPeer, result.errorString);
                emit actionDeliveryFail(action);
            }
        });

    return 0;
}

void EventMessageBus::at_actionReceived(const vms::event::AbstractActionPtr& action)
{
    emit actionReceived(action);
}

} // namespace event
} // namespace vms::server
} // namespace nx
