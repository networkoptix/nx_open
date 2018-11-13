#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <set>

#include <QtCore/QString>
#include <utils/common/threadqueue.h>

#include <nx/fusion/serialization/json.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/socket_common.h>
#include <nx/network/socket_global.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/counter.h>
#include <nx/vms/api/data/email_settings_data.h>

#include "notification.h"

namespace nx::cdb {

namespace conf { class Settings; }

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
        vms::api::EmailSettingsData email;
        std::function<void( bool )> completionHandler;
    };

    const conf::Settings& m_settings;
    bool m_terminated;
    mutable QnMutex m_mutex;
    nx::utils::Url m_notificationModuleUrl;
    std::set<nx::network::http::AsyncHttpClientPtr> m_ongoingRequests;
    nx::utils::Counter m_startedAsyncCallsCounter;
    std::atomic<std::uint64_t> m_notificationSequence;

    void onSendNotificationRequestDone(
        nx::utils::Counter::ScopedIncrement asyncCallLocker,
        nx::network::http::AsyncHttpClientPtr client,
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

} // namespace nx::cdb
