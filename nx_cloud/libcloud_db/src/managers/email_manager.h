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
#include <nx/fusion/serialization/json.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/socket_common.h>
#include <nx/network/socket_global.h>
#include <nx/utils/thread/mutex.h>
#include <utils/common/counter.h>

#include "settings.h"


namespace nx {
namespace cdb {

class AbstractEmailManager
{
public:
    virtual ~AbstractEmailManager() {}

    template<class NotificationType>
    void sendAsync(
        NotificationType notification,
        std::function<void(bool)> completionHandler)
    {
        sendAsync(
            QJson::serialized(notification),
            std::move(completionHandler));
    }

    virtual void sendAsync(
        QByteArray serializedNotification,
        std::function<void(bool)> completionHandler) = 0;
};

//!Responsible for sending emails
class EMailManager
:
    public AbstractEmailManager
{
public:
    EMailManager(const conf::Settings& settings) throw(std::runtime_error);
    virtual ~EMailManager();

protected:
    virtual void sendAsync(
        QByteArray serializedNotification,
        std::function<void(bool)> completionHandler) override;

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
    QnCounter m_startedAsyncCallsCounter;

    void onSendNotificationRequestDone(
        QnCounter::ScopedIncrement asyncCallLocker,
        nx_http::AsyncHttpClientPtr client,
        std::function<void(bool)> completionHandler);
};

/*!
    \note Access to internal factory func is not synchronized
*/
class EMailManagerFactory
{
public:
    static std::unique_ptr<AbstractEmailManager> create(const conf::Settings& settings);
    static void setFactory(
        std::function<std::unique_ptr<AbstractEmailManager>(
            const conf::Settings& settings)> factoryFunc);
};

}   //cdb
}   //nx

#endif  //NX_CLOUD_DB_EMAIL_MANAGER_H
