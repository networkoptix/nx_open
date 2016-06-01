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
#include <nx/network/cloud/cdb_endpoint_fetcher.h>
#include <nx/utils/log/log.h>

#include "notification.h"


namespace nx {
namespace cdb {

EMailManager::EMailManager( const conf::Settings& settings ) throw(std::runtime_error)
:
    m_settings( settings ),
    m_terminated( false )
{
    if (!m_settings.notification().serviceEndpoint.isEmpty())
    {
        m_notificationModuleEndpoint = SocketAddress(m_settings.notification().serviceEndpoint);
        return;
    }

    nx::network::cloud::CloudModuleEndPointFetcher endPointFetcher(
        "notification_module",
        std::make_unique<nx::network::cloud::RandomEndpointSelector>());
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
}

void EMailManager::sendAsync(
    QByteArray serializedNotification,
    std::function<void(bool)> completionHandler)
{
    auto asyncOperationLocker = m_startedAsyncCallsCounter.getScopedIncrement();

    if (!m_settings.notification().enabled)
    {
        if (completionHandler)
        {
            nx::network::SocketGlobals::aioService().post(
                [asyncOperationLocker, completionHandler]() {
                completionHandler(false);
            });
        }
        return;
    }

    QUrl url;
    url.setScheme("http");
    url.setHost(m_notificationModuleEndpoint.address.toString());
    url.setPort(m_notificationModuleEndpoint.port);
    url.setPath("/notifications/send");

    auto httpClient = nx_http::AsyncHttpClient::create();
    QObject::connect(
        httpClient.get(), &nx_http::AsyncHttpClient::done,
        httpClient.get(),
        [this, asyncOperationLocker, completionHandler](nx_http::AsyncHttpClientPtr client) {
        onSendNotificationRequestDone(
            std::move(asyncOperationLocker),
            std::move(client),
            std::move(completionHandler));
    },
        Qt::DirectConnection);
    {
        QnMutexLocker lk(&m_mutex);
        m_ongoingRequests.insert(httpClient);
    }
    httpClient->doPost(
        url,
        Qn::serializationFormatToHttpContentType(Qn::JsonFormat),
        std::move(serializedNotification));
}

void EMailManager::onSendNotificationRequestDone(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    nx_http::AsyncHttpClientPtr client,
    std::function<void(bool)> completionHandler)
{
    if (client->failed())
    {
        NX_LOGX(lm("Failed (1) to send email notification. %1")
            .arg(SystemError::toString(client->lastSysErrorCode())), cl_logDEBUG1);
    }
    else if (!nx_http::StatusCode::isSuccessCode(client->response()->statusLine.statusCode))
    {
        NX_LOGX(lm("Failed (2) to send email notification. Received %1(%2) response")
            .arg(client->response()->statusLine.statusCode).arg(client->response()->statusLine.reasonPhrase),
            cl_logDEBUG1);
    }

    if (completionHandler)
        completionHandler(
            client->response() &&
            (client->response()->statusLine.statusCode / 100 == nx_http::StatusCode::ok / 100));

    {
        QnMutexLocker lk(&m_mutex);
        m_ongoingRequests.erase(client);
    }
}


static std::function<std::unique_ptr<AbstractEmailManager>(
    const conf::Settings& settings)> kEMailManagerFactoryFunc;

std::unique_ptr<AbstractEmailManager> EMailManagerFactory::create(const conf::Settings& settings)
{
    return kEMailManagerFactoryFunc
        ? kEMailManagerFactoryFunc(settings)
        : std::make_unique<EMailManager>(settings);
}

void EMailManagerFactory::setFactory(
    std::function<std::unique_ptr<AbstractEmailManager>(
        const conf::Settings& settings)> factoryFunc)
{
    kEMailManagerFactoryFunc = std::move(factoryFunc);
}

}   //cdb
}   //nx
