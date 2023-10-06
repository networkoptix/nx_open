// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>
#include <list>
#include <vector>

#include <nx/utils/log/assert.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/thread_util.h>
#include <nx/utils/thread/wait_condition.h>

namespace nx::utils {

using SubscriptionId = std::size_t;

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
    using NotificationCallback = nx::utils::MoveOnlyFunc<void(Data...)>;

    ~Subscription()
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        NX_CRITICAL(m_eventReportingThread == 0);
        NX_CRITICAL(m_runningSubscriptionIds.empty());
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
        NX_MUTEX_LOCKER lk(&m_mutex);
        if (m_previousSubscriptionId == kInvalidSubscriptionId)
            ++m_previousSubscriptionId;
        *subscriptionId = m_previousSubscriptionId++;
        m_handlers.emplace(*subscriptionId, std::move(handler));
    }

    /**
     * @param subscriptionId Value returned by Subscription::subscribe
     * NOTE: Can be safely called within an event handler
     * NOTE: If event handler is running in another thread, blocks until handler has returned
     */
    void removeSubscription(SubscriptionId subscriptionId)
    {
        NX_MUTEX_LOCKER lk(&m_mutex);

        while (m_eventReportingThread != 0 &&                    //< Event handler is running.
            m_eventReportingThread != currentThreadSystemId() && //< Running not in current thread.
            nx::utils::contains(m_runningSubscriptionIds, subscriptionId))
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
        NX_MUTEX_LOCKER lock(&m_mutex);

        m_eventReportingThread = currentThreadSystemId();
        ++m_recursionDepth;
        auto guard = makeScopeGuard([this, &lock]()
        {
            if ((--m_recursionDepth) == 0)
                m_eventReportingThread = 0;

            lock.unlock();
            m_cond.wakeAll();
        });

        for (auto currentSubscriptionIter = m_handlers.begin();
             currentSubscriptionIter != m_handlers.end();
             )
        {
            m_runningSubscriptionIds.push_back(currentSubscriptionIter->first);
            auto anotherGuard = makeScopeGuard([this, &currentSubscriptionIter, &lock]()
            {
                lock.relock();
                currentSubscriptionIter = m_handlers.upper_bound(m_runningSubscriptionIds.back());
                m_runningSubscriptionIds.pop_back();
            });

            lock.unlock();
            m_cond.wakeAll();
            currentSubscriptionIter->second(data...);
        }
    }

private:
    nx::Mutex m_mutex;
    nx::WaitCondition m_cond;
    std::map<SubscriptionId, NotificationCallback> m_handlers;
    SubscriptionId m_previousSubscriptionId = kInvalidSubscriptionId;
    uintptr_t m_eventReportingThread = 0;
    std::vector<SubscriptionId> m_runningSubscriptionIds;
    int m_recursionDepth = 0;
};

} // namespace nx::utils
