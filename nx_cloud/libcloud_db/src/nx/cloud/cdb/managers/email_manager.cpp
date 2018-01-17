#include "email_manager.h"

#include <chrono>
#include <future>

#include <nx/email/mustache/mustache_helper.h>
#include <nx/email/email_manager_impl.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/aio/aio_service.h>
#include <nx/network/cloud/cloud_module_url_fetcher.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "notification.h"
#include "../settings.h"

namespace nx {
namespace cdb {

EMailManager::EMailManager(const conf::Settings& settings):
    m_settings(settings),
    m_terminated(false),
    m_notificationSequence(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count())
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
    auto currentNotificationIndex = ++m_notificationSequence;
    NX_LOGX(lm("Sending notification %1. %2")
        .arg(currentNotificationIndex).arg(notification.serializeToJson()),
        cl_logDEBUG2);

    auto asyncOperationLocker = m_startedAsyncCallsCounter.getScopedIncrement();

    if (!m_settings.notification().enabled)
    {
        if (completionHandler)
        {
            nx::network::SocketGlobals::aioService().post(
                [asyncOperationLocker, completionHandler = std::move(completionHandler)]()
                {
                    completionHandler(false);
                });
        }
        return;
    }

    auto httpClient = nx::network::http::AsyncHttpClient::create();
    QObject::connect(
        httpClient.get(), &nx::network::http::AsyncHttpClient::done,
        httpClient.get(),
        [this, asyncOperationLocker, currentNotificationIndex,
            completionHandler = std::move(completionHandler)](
                nx::network::http::AsyncHttpClientPtr client)
        {
            onSendNotificationRequestDone(
                std::move(asyncOperationLocker),
                std::move(client),
                currentNotificationIndex,
                std::move(completionHandler));
        },
        Qt::DirectConnection);

    {
        QnMutexLocker lk(&m_mutex);
        m_ongoingRequests.insert(httpClient);
    }

    auto notificationUrl = m_notificationModuleUrl;
    notificationUrl.setQuery(lm("id=%1").arg(currentNotificationIndex));

    httpClient->doPost(
        notificationUrl,
        Qn::serializationFormatToHttpContentType(Qn::JsonFormat),
        notification.serializeToJson());
}

void EMailManager::onSendNotificationRequestDone(
    nx::utils::Counter::ScopedIncrement /*asyncCallLocker*/,
    nx::network::http::AsyncHttpClientPtr client,
    std::uint64_t notificationIndex,
    std::function<void(bool)> completionHandler)
{
    if (client->failed())
    {
        NX_LOGX(lm("Failed (1) to send email notification %1. %2")
            .arg(notificationIndex).arg(SystemError::toString(client->lastSysErrorCode())),
            cl_logERROR);
    }
    else if (!nx::network::http::StatusCode::isSuccessCode(client->response()->statusLine.statusCode))
    {
        NX_LOGX(lm("Failed (2) to send email notification %1. Received %2(%3) response")
            .arg(notificationIndex).arg(client->response()->statusLine.statusCode)
            .arg(client->response()->statusLine.reasonPhrase),
            cl_logERROR);
    }
    else
    {
        NX_LOGX(lm("Successfully sent notification %1").arg(notificationIndex), cl_logDEBUG2);
    }

    if (completionHandler)
    {
        completionHandler(
            client->response() &&
            nx::network::http::StatusCode::isSuccessCode(client->response()->statusLine.statusCode));
    }

    QnMutexLocker lk(&m_mutex);
    m_ongoingRequests.erase(client);
}

//-------------------------------------------------------------------------------------------------
// EMailManagerFactory

static EMailManagerFactory::FactoryFunc eMailManagerFactoryFunc;

std::unique_ptr<AbstractEmailManager> EMailManagerFactory::create(const conf::Settings& settings)
{
    return eMailManagerFactoryFunc
        ? eMailManagerFactoryFunc(settings)
        : std::make_unique<EMailManager>(settings);
}

EMailManagerFactory::FactoryFunc EMailManagerFactory::setFactory(FactoryFunc factoryFunc)
{
    auto previousFunc = std::move(eMailManagerFactoryFunc);
    eMailManagerFactoryFunc = std::move(factoryFunc);
    return previousFunc;
}

} // namespace cdb
} // namespace nx
