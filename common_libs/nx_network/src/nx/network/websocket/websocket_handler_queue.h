#pragma once

#include <queue>
#include <stdexcept>
#include "websocket_common_types.h"

namespace nx {
namespace network {
namespace websocket {

template<typename BufferType>
class HandlerQueue
{
public:
    struct Held
    {
        IoCompletionHandler handler;
        BufferType buffer;

        Held(IoCompletionHandler&& handler, BufferType&& buffer) :
            handler(std::move(handler)),
            buffer(std::move(buffer))
        {}

        Held(Held&& /*other*/) = default;
        Held& operator=(Held&& /*other*/) = default;

        Held() {}
    };

private:
    using QueueType = std::queue<Held>;

public:
    HandlerQueue():
        m_firstUsed(false)
    {}

    HandlerQueue(const HandlerQueue& other) = delete;
    HandlerQueue(HandlerQueue&& other) = default;

    template<typename... Args>
    void emplace(Args&&... args)
    {
        if (m_firstUsed)
        {
            m_queue.emplace(std::forward<Args>(args)...);
            return;
        }

        m_first = Held(std::forward<Args>(args)...);
        m_firstUsed = true;
    }

    Held pop()
    {
        NX_ASSERT(m_firstUsed);
        auto retVal = std::move(m_first);
        if (!m_queue.empty())
        {
            m_first = std::move(m_queue.front());
            m_queue.pop();
        }
        else
            m_firstUsed = false;

        return retVal;
    }

    const Held& first() const
    {
        return m_first;
    }

    bool empty() const
    {
        return !m_firstUsed;
    }

    void clear()
    {
        m_first = Held();
        m_firstUsed = false;
        QueueType tmp;
        m_queue.swap(tmp);
    }

private:
    Held m_first;
    bool m_firstUsed;
    QueueType m_queue;
};

}
}
}
