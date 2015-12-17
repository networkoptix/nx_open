/**********************************************************
* Aug 12, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CLOUD_DB_EMAIL_MANAGER_H
#define NX_CLOUD_DB_EMAIL_MANAGER_H

#include <functional>
#include <memory>
#include <thread>
#include <set>

#include <QtCore/QString>

#include <nx_ec/data/api_email_data.h>
#include <utils/common/threadqueue.h>
#include <utils/serialization/json.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/socket_common.h>
#include <nx/network/socket_global.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/thread_safe_counter.h>

#include "settings.h"


namespace nx {
namespace cdb {

//!Responsible for sending emails
class EMailManager
{
public:
    EMailManager( const conf::Settings& settings ) throw(std::runtime_error);
    virtual ~EMailManager();

    template<class NotificationType>
    void sendAsync(
        NotificationType notification,
        std::function<void(bool)> completionHandler)
    {
        auto asyncOperationLocker = m_startedAsyncCallsCounter.getScopedIncrement();

        if (!m_settings.notification().enabled)
        {
            if (completionHandler)
            {
                nx::SocketGlobals::aioService().post(
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
            QJson::serialized(std::move(notification)));
    }

private:
    struct SendEmailTask
    {
        ec2::ApiEmailData email;
        std::function<void( bool )> completionHandler;
    };
    
    const conf::Settings& m_settings;
    bool m_terminated;
    mutable QnMutex m_mutex;
    SocketAddress m_notificationModuleEndpoint;
    std::set<nx_http::AsyncHttpClientPtr> m_ongoingRequests;
    ThreadSafeCounter m_startedAsyncCallsCounter;

    void onSendNotificationRequestDone(
        ThreadSafeCounter::ScopedIncrement asyncCallLocker,
        nx_http::AsyncHttpClientPtr client,
        std::function<void(bool)> completionHandler);
};

}   //cdb
}   //nx

#endif  //NX_CLOUD_DB_EMAIL_MANAGER_H
