#include "ordered_requests_manager.h"

#include <api/server_rest_connection.h>

namespace nx {
namespace vms {
namespace client {
namespace core {

struct OrderedRequestsManager::Private
{
    using Request = std::function<rest::Handle ()>;
    using Requests = QList<Request>;

    void addRequest(const Request& request);
    void tryExecuteNextRequest();

    OrderedRequestsManager::CallbackType makeInternalCallback(
        const OrderedRequestsManager::CallbackType& callback);

    rest::Handle currentHandle = -1;
    Requests requests;
};

void OrderedRequestsManager::Private::addRequest(const Request& request)
{
    if (!request)
        return;

    requests.push_back(request);
    tryExecuteNextRequest();
}

void OrderedRequestsManager::Private::tryExecuteNextRequest()
{
    if (currentHandle > 0 || requests.isEmpty())
        return;

    currentHandle = requests.front()();
    requests.pop_front();
}

OrderedRequestsManager::CallbackType OrderedRequestsManager::Private::makeInternalCallback(
    const OrderedRequestsManager::CallbackType& callback)
{
    const auto internalCallback =
        [this, callback](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            if (handle != currentHandle)
            {
                NX_ASSERT(false, "Wrong requests order");
                return;
            }

            if (callback)
                callback(success, handle, result);

            currentHandle = -1;
            tryExecuteNextRequest();
        };

    return internalCallback;
}

//--------------------------------------------------------------------------------------------------

OrderedRequestsManager::OrderedRequestsManager():
    d(new Private())
{
}

OrderedRequestsManager::~OrderedRequestsManager()
{
}

void OrderedRequestsManager::sendSoftwareTriggerCommand(
    const rest::QnConnectionPtr& connection,
    const QnUuid& cameraId,
    const QString& triggerId,
    nx::vms::api::EventState toggleState,
    const CallbackType& callback,
    QThread* targetThread)
{
    if (!connection)
        return;

    const auto request =
        [this, connection, cameraId, triggerId, toggleState, callback, targetThread]()
        {
            return connection->softwareTriggerCommand(
                cameraId,
                triggerId,
                toggleState,
                d->makeInternalCallback(callback),
                targetThread);
        };

    d->addRequest(request);
}

void OrderedRequestsManager::sendTwoWayAudioCommand(
    const rest::QnConnectionPtr& connection,
    const QString& sourceId,
    const QnUuid& cameraId,
    bool start,
    const CallbackType& callback,
    QThread* targetThread)
{
    if (!connection)
        return;

    const auto request =
        [connection, sourceId, cameraId, start, callback, targetThread]()
        {
            return connection->twoWayAudioCommand(sourceId, cameraId, start, callback, targetThread);
        };

    d->addRequest(request);
}

} // namespace core
} // namespace client
} // namespace vms
} // namespace nx

