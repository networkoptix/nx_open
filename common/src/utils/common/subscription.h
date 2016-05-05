#ifndef SUBSCRIPTION_H
#define SUBSCRIPTION_H

#include "guard.h"

#include <mutex>
#include <list>

/** Subscribtion-notification model event */
template <typename /*...*/ Data>  // TODO: uncomment dots when variadic templates are avaliable
class Subscription
{
public:
    /** Notification callback */
    typedef std::function<void(Data /*...*/)> Handler;

    /** Subscribes @param hadler for this event.
     *  NOTE: do not use blocking operations inside @param hadler */
    Guard subscribe(const Handler& handler)
    {
        std::lock_guard<std::mutex> g(m_mutex);
        auto it = m_handlers.insert(m_handlers.end(), handler);
        return Guard([this, it]()
        {
             std::lock_guard<std::mutex> g(m_mutex);
             m_handlers.erase(it);
        });
    }

    /** Notifies all subscribers about the event */
    void notify(Data /*...*/ data)
    {
        std::lock_guard<std::mutex> g(m_mutex);
        for (const auto& handler : m_handlers)
            handler(data /*...*/);
    }

private:
    std::mutex m_mutex;
    std::list<Handler> m_handlers;
};

#endif // SUBSCRIPTION_H
