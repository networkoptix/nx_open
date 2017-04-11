#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <set>

#include <QtCore/QString>
#include <QtCore/QUrl>

#include <nx_ec/data/api_email_data.h>
#include <utils/common/threadqueue.h>
#include <nx/fusion/serialization/json.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/socket_common.h>
#include <nx/network/socket_global.h>
#include <nx/utils/thread/mutex.h>
#include <utils/common/counter.h>

#include "notification.h"
#include "settings.h"

namespace nx {
namespace cdb {

namespace conf {
class Settings;
} // namespace conf

class AbstractEmailManager
{
public:
    virtual ~AbstractEmailManager() {}

    virtual void sendAsync(
        const AbstractNotification& notification,
        std::function<void(bool)> completionHandler) = 0;
};

//!Responsible for sending emails
class EMailManager:
    public AbstractEmailManager
{
public:
    EMailManager(const conf::Settings& settings);
    virtual ~EMailManager();

protected:
    virtual void sendAsync(
        const AbstractNotification& notification,
        std::function<void(bool)> completionHandler) override;

private:
    struct SendEmailTask
    {
        ::ec2::ApiEmailData email;
        std::function<void( bool )> completionHandler;
    };
    
    const conf::Settings& m_settings;
    bool m_terminated;
    mutable QnMutex m_mutex;
    QUrl m_notificationModuleUrl;
    std::set<nx_http::AsyncHttpClientPtr> m_ongoingRequests;
    QnCounter m_startedAsyncCallsCounter;
    std::atomic<std::uint64_t> m_notificationSequence;

    void onSendNotificationRequestDone(
        QnCounter::ScopedIncrement asyncCallLocker,
        nx_http::AsyncHttpClientPtr client,
        std::uint64_t notificationIndex,
        std::function<void(bool)> completionHandler);
};

/**
 * @note Access to internal factory func is not synchronized.
 */
class EMailManagerFactory
{
public:
    using FactoryFunc =
        std::function<std::unique_ptr<AbstractEmailManager>(const conf::Settings& settings)>;

    static std::unique_ptr<AbstractEmailManager> create(const conf::Settings& settings);
    /**
     * Default is nullptr.
     * @return Previous factory func.
     */
    static FactoryFunc setFactory(FactoryFunc factoryFunc);
};

} // namespace cdb
} // namespace nx
