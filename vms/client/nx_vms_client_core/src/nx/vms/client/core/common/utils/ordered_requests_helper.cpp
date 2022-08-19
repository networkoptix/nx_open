// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ordered_requests_helper.h"

#include <QtCore/QQueue>

#include <api/server_rest_connection.h>

#include <nx/utils/guarded_callback.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::core {

struct OrderedRequestsHelper::Private
{
    using Request = std::function<rest::Handle ()>;
    using Requests = QQueue<Request>;

    void addRequest(const Request& request);
    void tryExecuteNextRequest();

    rest::Handle currentHandle = -1;
    Requests requests;
};

void OrderedRequestsHelper::Private::addRequest(const Request& request)
{
    if (!request)
        return;

    requests.enqueue(request);
    tryExecuteNextRequest();
}

void OrderedRequestsHelper::Private::tryExecuteNextRequest()
{
    while (currentHandle == -1 && !requests.isEmpty())
        currentHandle = requests.dequeue()();
}

//--------------------------------------------------------------------------------------------------

OrderedRequestsHelper::OrderedRequestsHelper(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

OrderedRequestsHelper::~OrderedRequestsHelper()
{
}

bool OrderedRequestsHelper::postJsonResult(
    const rest::ServerConnectionPtr& connection,
    const QString& action,
    const nx::network::rest::Params& params,
    rest::JsonResultCallback&& callback,
    QThread* thread)
{
    if (!connection)
        return false;

    const auto internalCallback = nx::utils::guarded(this,
        [this, callback](
            bool success, rest::Handle handle, const nx::network::rest::JsonResult& result)
        {
            if (handle != d->currentHandle)
            {
                NX_ASSERT(false, "Wrong requests order");
                return;
            }

            if (callback)
                callback(success, handle, result);

            d->currentHandle = -1;
            d->tryExecuteNextRequest();
        });

    const auto request =
        [connection, action, params, callback, internalCallback, thread]() -> rest::Handle
        {
            if (connection)
                return connection->postJsonResult(action, params, {}, internalCallback, thread);

            executeInThread(thread,
                [callback]() { callback(/*success*/ false, /*handle*/ -1, /*result*/ {}); });

            return -1;
        };

    d->addRequest(request);
    return true;
}

} // namespace nx::vms::client::core

