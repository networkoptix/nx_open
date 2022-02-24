// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

#include "sync_queue.h"
#include "../move_only_func.h"

namespace nx {
namespace utils {

template<typename Item>
class SyncQueueWithItemStayTimeout
{
public:
    typedef nx::utils::MoveOnlyFunc<void(Item)> ItemStayTimeoutHandler;

    /**
     * @param timeoutHandler This handler is called from SyncQueueWithItemStayTimeout::pop,
     * so it MUST NOT block
     */
    void enableItemStayTimeoutEvent(
        std::chrono::milliseconds timeout,
        ItemStayTimeoutHandler itemStayTimeoutHandler)
    {
        m_itemStayTimeout = timeout;
        m_itemStayTimeoutHandler = std::move(itemStayTimeoutHandler);
    }

    void push(Item item)
    {
        m_items.push({std::move(item), std::chrono::steady_clock::now()});
    }

    QueueReaderId generateReaderId()
    {
        return m_items.generateReaderId();
    }

    /**
     * Returns first item for which conditionFunc(...) condition is met.
     * If none, blocks until new element is added or retestPopCondition is called.
     */
    template<typename ConditionFunc>
    std::optional<Item> popIf(
        ConditionFunc conditionFunc,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt,
        QueueReaderId readerId = kInvalidQueueReaderId)
    {
        using namespace std::chrono;

        std::optional<steady_clock::time_point> timeToWaitUntil;
        if (timeout)
            timeToWaitUntil = std::chrono::steady_clock::now() + *timeout;
        for (;;)
        {
            const auto currentTime = steady_clock::now();
            if (timeToWaitUntil && (*timeToWaitUntil <= currentTime))
                return std::nullopt;

            auto popTimeout = milliseconds::zero();
            if (timeToWaitUntil)
                popTimeout = duration_cast<milliseconds>(*timeToWaitUntil - currentTime);
            auto itemContext = m_items.popIf(
                [&conditionFunc](const ItemContext& value)
                {
                    return conditionFunc(value.data);
                },
                popTimeout,
                readerId);
            if (!itemContext)
                return std::nullopt;

            if (m_itemStayTimeout &&
                (std::chrono::steady_clock::now() - itemContext->insertionTime
                    >= *m_itemStayTimeout))
            {
                m_itemStayTimeoutHandler(std::move(itemContext->data));
                continue;
            }
            return std::move(itemContext->data);
        }
    }

    std::optional<Item> pop(
        std::optional<std::chrono::milliseconds> timeout = std::nullopt,
        QueueReaderId readerId = kInvalidQueueReaderId)
    {
        return popIf([](const Item&) { return true; }, timeout, readerId);
    }

    void retestPopIfCondition()
    {
        m_items.retestPopIfCondition();
    }

    std::size_t size() const
    {
        return m_items.size();
    }

    void addReaderToTerminatedList(QueueReaderId readerId)
    {
        m_items.addReaderToTerminatedList(readerId);
    }

    void removeReaderFromTerminatedList(QueueReaderId readerId)
    {
        m_items.removeReaderFromTerminatedList(readerId);
    }

private:
    struct ItemContext
    {
        Item data;
        std::chrono::steady_clock::time_point insertionTime;
    };

    std::optional<std::chrono::milliseconds> m_itemStayTimeout;
    SyncQueue<ItemContext> m_items;
    ItemStayTimeoutHandler m_itemStayTimeoutHandler;
};

} // namespace utils
} // namespace nx
