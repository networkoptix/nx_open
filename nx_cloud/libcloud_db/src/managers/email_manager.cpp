/**********************************************************
* Aug 12, 2015
* a.kolesnikov
***********************************************************/

#include "email_manager.h"

#include <future>

#include <api/global_settings.h>
#include <common/common_globals.h>
#include <nx/utils/std/cpp14.h>
#include <nx/fusion/model_functions.h>

#include <nx/email/mustache/mustache_helper.h>
#include <nx/email/email_manager_impl.h>
#include <nx/network/cloud/cloud_module_url_fetcher.h>
#include <nx/utils/log/log.h>

#include "notification.h"


namespace nx {
namespace cdb {

EMailManager::EMailManager( const conf::Settings& settings )
:
    m_settings( settings ),
    m_terminated( false )
{
    m_notificationModuleUrl = m_settings.notification().url;
}

EMailManager::~EMailManager()
{
    //NOTE if we just terminate all ongoing calls then user of this class can dead-lock
    m_startedAsyncCallsCounter.wait();
}

void EMailManager::sendAsync(
    const AbstractNotification& notification,
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

    auto httpClient = nx_http::AsyncHttpClient::create();
    QObject::connect(
        httpClient.get(), &nx_http::AsyncHttpClient::done,
        httpClient.get(),
        [this, asyncOperationLocker, completionHandler](
            nx_http::AsyncHttpClientPtr client)
        {
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
        m_notificationModuleUrl,
        Qn::serializationFormatToHttpContentType(Qn::JsonFormat),
        notification.serializeToJson());
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
