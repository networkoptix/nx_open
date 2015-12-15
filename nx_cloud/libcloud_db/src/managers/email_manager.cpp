/**********************************************************
* Aug 12, 2015
* a.kolesnikov
***********************************************************/

#include "email_manager.h"

#include <future>

#include <api/global_settings.h>
#include <common/common_globals.h>
#include <utils/common/cpp14.h>
#include <utils/common/model_functions.h>

#include <nx/email/mustache/mustache_helper.h>
#include <nx/email/email_manager_impl.h>
#include <nx/network/cloud_connectivity/cdb_endpoint_fetcher.h>
#include <nx/utils/log/log.h>

#include "notification.h"


namespace nx {
namespace cdb {

EMailManager::EMailManager( const conf::Settings& settings ) throw(std::runtime_error)
:
    m_settings( settings ),
    m_terminated( false )
{
    nx::cc::CloudModuleEndPointFetcher endPointFetcher(
        "notification_module",
        std::make_unique<nx::cc::RandomEndpointSelector>());
    std::promise<nx_http::StatusCode::Value> endpointPromise;
    auto endpointFuture = endpointPromise.get_future();
    endPointFetcher.get(
        [&endpointPromise, this](
            nx_http::StatusCode::Value resCode,
            SocketAddress endpoint)
    {
        endpointPromise.set_value(resCode);
        m_notificationModuleEndpoint = std::move(endpoint);
    });
    if (endpointFuture.get() != nx_http::StatusCode::ok)
        throw std::runtime_error("Failed to find out notification module address");
}

EMailManager::~EMailManager()
{
    //NOTE if we just terminate all ongoing calls then user of this class can dead-lock
    m_startedAsyncCallsCounter.wait();

    //decltype(m_ongoingRequests) ongoingRequests;
    //{
    //    QnMutexLocker lk(&m_mutex);
    //    ongoingRequests = std::move(m_ongoingRequests);
    //}

    //for (auto httpRequest: ongoingRequests)
    //    httpRequest->terminate();
}

void EMailManager::onSendNotificationRequestDone(
    ThreadSafeCounter::ScopedIncrement asyncCallLocker,
    nx_http::AsyncHttpClientPtr client,
    std::function<void(bool)> completionHandler)
{
    if (completionHandler)
        completionHandler(
            client->response() &&
            (client->response()->statusLine.statusCode / 100 == nx_http::StatusCode::ok / 100));

    {
        QnMutexLocker lk(&m_mutex);
        m_ongoingRequests.erase(client);
    }
}

}   //cdb
}   //nx
