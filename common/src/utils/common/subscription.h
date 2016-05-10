
#pragma once

#include <cstdint>
#include <mutex>
#include <list>

#include <nx/utils/log/assert.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/thread_util.h>

#include "guard.h"


namespace nx {
namespace utils {

typedef std::size_t SubscriptionId;

constexpr static const SubscriptionId kInvalidSubscriptionId = 0;

/** Subscribtion-notification model event.
    \note All methods are thread-safe
*/
template <typename... Data>
class Subscription
{
    typedef Subscription<Data...> SelfType;

public:
    /** Notification callback */
    typedef nx::utils::MoveOnlyFunc<void(Data...)> Handler;

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

    /** Subscribes @param hadler for this event.
     *  @param subscriptionId This value can be used to unsubscribe
     *  NOTE: do not use blocking operations inside @param hadler
     */
    void subscribe(Handler handler, SubscriptionId* const subscriptionId)
    {
        QnMutexLocker lk(&m_mutex);
        if (m_previousSubscriptionId == kInvalidSubscriptionId)
            ++m_previousSubscriptionId;
        *subscriptionId = m_previousSubscriptionId++;
        m_handlers.emplace(*subscriptionId, std::move(handler));
    }

    /** 
        @param subscriptionId Value returned by \a Subscription::subscribe
        \note Can be safely called with in event handler
        \note If event handler is running in another thread, blocks until handler has returned
    */
    void removeSubscription(SubscriptionId subscriptionId)
    {
        QnMutexLocker lk(&m_mutex);

        while (m_eventReportingThread != 0 &&        //event handler is running
            m_eventReportingThread != currentThreadSystemId() &&      //running not in current thread
            m_runningSubscriptionId == subscriptionId)
        {
            //waiting for handler to complete
            m_cond.wait(lk.mutex());
        }

        m_handlers.erase(subscriptionId);
    }

    /** Notifies all subscribers about the event */
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
            currentSubscriptionIter = m_handlers.upper_bound(m_runningSubscriptionId);
        }

        m_runningSubscriptionId = kInvalidSubscriptionId;
        m_eventReportingThread = 0;
    }

private:
    QnMutex m_mutex;
    QnWaitCondition m_cond;
    std::map<SubscriptionId, Handler> m_handlers;
    SubscriptionId m_previousSubscriptionId;
    uintptr_t m_eventReportingThread;
    SubscriptionId m_runningSubscriptionId;
};

}   //namespace utils
}   //namespace nx
