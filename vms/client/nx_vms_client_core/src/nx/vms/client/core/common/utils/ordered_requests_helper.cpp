// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ordered_requests_helper.h"

#include <QtCore/QQueue>

#include <api/server_rest_connection.h>
#include <nx/utils/guarded_callback.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::core {

namespace {

static rest::Handle kNoConnection = -2;

} // namespace

struct OrderedRequestsHelper::Private
{
    using Request = nx::utils::MoveOnlyFunc<rest::Handle()>;
    using Requests = std::deque<Request>;

    void addRequest(Request&& request);
    void tryExecuteNextRequest();

    rest::Handle currentHandle = -1;
    Requests requests;
};

void OrderedRequestsHelper::Private::addRequest(Request&& request)
{
    if (!request)
        return;

    requests.push_back(std::move(request));
    tryExecuteNextRequest();
}

void OrderedRequestsHelper::Private::tryExecuteNextRequest()
{
    while (currentHandle == -1 && !requests.empty())
    {
        auto callback = std::move(requests.front());
        requests.pop_front();
        currentHandle = callback();
    }
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

    auto internalCallback = nx::utils::guarded(this,
        [this, callback = std::move(callback)](
            bool success, rest::Handle handle, const nx::network::rest::JsonResult& result)
        {
            if (handle != d->currentHandle && handle != kNoConnection)
            {
                NX_ASSERT(false, "Wrong requests order");
                return;
            }

            if (callback)
                callback(success, handle == kNoConnection ? -1 : handle, result);

            d->currentHandle = -1;

            if (handle != kNoConnection)
                d->tryExecuteNextRequest();
        });

    auto request =
        [connection, action, params, internalCallback = std::move(internalCallback),
            thread]() mutable -> rest::Handle
        {
            if (connection)
            {
                return connection->postJsonResult(
                    action, params, {}, std::move(internalCallback), thread);
            }

            executeInThread(thread,
                [callback = std::move(internalCallback)]()
                {
                    callback(
                        /*success*/ false,
                        kNoConnection,
                        /*result*/ nx::network::rest::JsonResult{});
                });

            return -1;
        };

    d->addRequest(std::move(request));
    return true;
}

bool OrderedRequestsHelper::getJsonResult(
    const rest::ServerConnectionPtr& connection,
    const QString& action,
    const nx::network::rest::Params& params,
    rest::JsonResultCallback&& callback,
    QThread* thread)
{
    if (!connection)
        return false;

    auto internalCallback = nx::utils::guarded(this,
        [this, callback = std::move(callback)](
            bool success, rest::Handle handle, const nx::network::rest::JsonResult& result)
        {
            if (handle != d->currentHandle && handle != kNoConnection)
            {
                NX_ASSERT(false, "Wrong requests order");
                return;
            }

            if (callback)
                callback(success, handle == kNoConnection ? -1 : handle, result);

            d->currentHandle = -1;

            if (handle != kNoConnection)
                d->tryExecuteNextRequest();
        });

    auto request =
        [connection, action, params, internalCallback = std::move(internalCallback),
            thread]() mutable -> rest::Handle
        {
            if (connection)
            {
                return connection->getJsonResult(
                    action, params, std::move(internalCallback), thread);
            }

            executeInThread(thread,
                [callback = std::move(internalCallback)]()
                {
                    callback(
                        /*success*/ false,
                        kNoConnection,
                        /*result*/ nx::network::rest::JsonResult{});
                });

            return -1;
        };

    d->addRequest(std::move(request));
    return true;
}

} // namespace nx::vms::client::core
