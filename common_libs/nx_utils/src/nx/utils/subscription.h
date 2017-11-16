#pragma once

#include <cstdint>
#include <mutex>
#include <list>

#include <nx/utils/log/assert.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/thread_util.h>
#include <nx/utils/scope_guard.h>

namespace nx {
namespace utils {

typedef std::size_t SubscriptionId;

constexpr static const SubscriptionId kInvalidSubscriptionId = 0;

/**
 * Subscription-notification model event.
 * NOTE: All methods are thread-safe.
 */
template <typename... Data>
class Subscription
{
    typedef Subscription<Data...> SelfType;

public:
    typedef nx::utils::MoveOnlyFunc<void(Data...)> NotificationCallback;

    Subscription()
    :
        m_previousSubscriptionId(kInvalidSubscriptionId),
        m_eventReportingThread(0),
        m_runningSubscriptionId(kInvalidSubscriptionId)
    {
    }

    ~Subscription()
    {
        QnMutexLocker lk(&m_mutex);
        NX_CRITICAL(m_eventReportingThread == 0);
        NX_CRITICAL(m_runningSubscriptionId == kInvalidSubscriptionId);
    }

    /** 
     * Subscribes handler for this event.
     * @param subscriptionId This value can be used to unsubscribe
     * NOTE: do not use blocking operations inside @param handler.
     */
    void subscribe(
        NotificationCallback handler,
        SubscriptionId* const subscriptionId)
    {
        QnMutexLocker lk(&m_mutex);
        if (m_previousSubscriptionId == kInvalidSubscriptionId)
            ++m_previousSubscriptionId;
        *subscriptionId = m_previousSubscriptionId++;
        m_handlers.emplace(*subscriptionId, std::move(handler));
    }

    /** 
     * @param subscriptionId Value returned by Subscription::subscribe
     * NOTE: Can be safely called with in event handler
     * NOTE: If event handler is running in another thread, blocks until handler has returned
     */
    void removeSubscription(SubscriptionId subscriptionId)
    {
        QnMutexLocker lk(&m_mutex);

        while (m_eventReportingThread != 0 &&                    //< Event handler is running.
            m_eventReportingThread != currentThreadSystemId() && //< Running not in current thread.
            m_runningSubscriptionId == subscriptionId)
        {
            // Waiting for handler to complete.
            m_cond.wait(lk.mutex());
        }

        m_handlers.erase(subscriptionId);
    }

    /** 
     * Notifies all subscribers about the event.
     */
    void notify(Data... data)
    {
        QnMutexLocker lk(&m_mutex);

        m_eventReportingThread = currentThreadSystemId();

        for (auto currentSubscriptionIter = m_handlers.begin();
             currentSubscriptionIter != m_handlers.end();
             )
        {
            m_runningSubscriptionId = currentSubscriptionIter->first;
            lk.unlock();
            m_cond.wakeAll();
            currentSubscriptionIter->second(data...);
            lk.relock();
            currentSubscriptionIter = 
                m_handlers.upper_bound(m_runningSubscriptionId);
        }

        m_runningSubscriptionId = kInvalidSubscriptionId;
        m_eventReportingThread = 0;

        lk.unlock();
        m_cond.wakeAll();
    }

private:
    QnMutex m_mutex;
    QnWaitCondition m_cond;
    std::map<SubscriptionId, NotificationCallback> m_handlers;
    SubscriptionId m_previousSubscriptionId;
    uintptr_t m_eventReportingThread;
    SubscriptionId m_runningSubscriptionId;
};

} // namespace utils
} // namespace nx
