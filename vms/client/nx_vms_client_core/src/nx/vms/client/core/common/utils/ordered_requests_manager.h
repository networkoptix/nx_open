#pragma once

#include <nx/utils/uuid.h>
#include <api/server_rest_connection_fwd.h>
#include <rest/server/json_rest_result.h>

namespace nx {
namespace vms {
namespace client {
namespace core {

class OrderedRequestsManager
{
public:
    OrderedRequestsManager();
    ~OrderedRequestsManager();

    using CallbackType = rest::Callback<QnJsonRestResult>;

    void sendSoftwareTriggerCommand(
        const rest::QnConnectionPtr& connection,
        const QnUuid& cameraId,
        const QString& triggerId,
        nx::vms::api::EventState toggleState,
        const CallbackType& callback,
        QThread* targetThread);

    void sendTwoWayAudioCommand(
        const rest::QnConnectionPtr& connection,
        const QString& sourceId,
        const QnUuid& cameraId,
        bool start,
        const CallbackType& callback,
        QThread* targetThread);

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace core
} // namespace client
} // namespace vms
} // namespace nx
