// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>

#include <nx/utils/async_operation_guard.h>
#include <nx/utils/thread/mutex.h>

#include "basic_pollable.h"
#include "detail/data_wrapper.h"
#include "timer.h"

namespace nx::network::aio {

/**
 * Wraps an asynchronous operation represented with a functor as if it was bound to an AIO thread
 * similar to sockets.
 * Introduced to move any asynchronous operation to AIO thread.
 * That means:
 * 1. Completion handler is called within aio thread
 * 2. Operation can be safely cancelled using API of nx::network::aio::BasicPollable.
 * Cancelled operation will still be running, but its completion is ignored
 * and does not access freed memory.
 * NOTE: Class methods are not thread-safe.
 */
template<typename Func>
class AsyncOperationWrapper:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

public:
    using OnTimeoutHandler = nx::utils::MoveOnlyFunc<void()>;

    template<typename _Func>
    AsyncOperationWrapper(_Func func):
        m_func(std::move(func))
    {
    }

    virtual ~AsyncOperationWrapper()
    {
        m_guard.reset();
    }

    /**
     * NOTE: Another operation can be started while the previous one is still running.
     */
    template<typename Handler, typename... Args>
    void invoke(Handler handler, Args&&... args)
    {
        invokeWithTimeout(
            std::move(handler),
            std::chrono::milliseconds::zero(),
            OnTimeoutHandler(),
            std::forward<Args>(args)...);
    }

    /**
     * @param timeout 0 is treated as "no timeout".
     */
    template<typename Handler, typename... Args>
    void invokeWithTimeout(
        Handler handler,
        std::chrono::milliseconds timeout,
        OnTimeoutHandler timedOutHandler,
        Args&&... args)
    {
        auto argsTuple = std::make_tuple(std::forward<Args>(args)...);
        const auto operationId = ++m_prevOperationId;

        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            auto it = m_idToOperationContext.emplace(operationId, OperationContext{}).first;
            it->second.saveHandler(std::move(handler));
        }

        auto processResult = prepareCompletionHandler<Handler>(operationId);

        std::apply(
            m_func,
            std::tuple_cat(std::move(argsTuple), std::make_tuple(std::move(processResult))));

        if (timeout > std::chrono::milliseconds::zero())
        {
            dispatch(
                [this, operationId, timeout,
                    timedOutHandler = std::move(timedOutHandler)]() mutable
                {
                    startTimer(operationId, timeout, std::move(timedOutHandler));
                });
        }
    }

protected:
    virtual void stopWhileInAioThread() override
    {
        base_type::stopWhileInAioThread();

        m_guard.reset();
        m_idToOperationContext.clear();
    }

private:
    class OperationContext
    {
    public:
        std::unique_ptr<network::aio::Timer> timer;
        OnTimeoutHandler onTimeoutHandler;

        template<typename Handler>
        void saveHandler(Handler handler)
        {
            m_handlerWrappers =
                std::make_unique<detail::DataWrapper<Handler>>(std::move(handler));
        }

        template<typename Handler>
        Handler takeHandler()
        {
            auto handler = static_cast<detail::DataWrapper<Handler>*>(
                m_handlerWrappers.get())->take();
            m_handlerWrappers.reset();

            return handler;
        }

    private:
        std::unique_ptr<detail::AbstractDataWrapper> m_handlerWrappers;
    };

    Func m_func;
    nx::utils::AsyncOperationGuard m_guard;
    std::atomic<int> m_prevOperationId{0};
    nx::Mutex m_mutex;
    std::map<int, OperationContext> m_idToOperationContext;

    template<typename Handler>
    auto prepareCompletionHandler(int operationId)
    {
        // This lambda is outside of the lambda below due to the bug in GCC, see
        // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80947#c17
        auto func = [this, operationId](auto args) mutable {
            OperationContext context;
            {
                const NX_MUTEX_LOCKER lock(&m_mutex);
                auto it = m_idToOperationContext.find(operationId);
                if (it == m_idToOperationContext.end())
                    return; //< the operation has already completed.
                context = std::move(it->second);
                m_idToOperationContext.erase(it);
            }

            auto handler = context.template takeHandler<Handler>();
            std::apply(handler, std::move(args));
        };
        return
            [this, sharedGuard = m_guard.sharedGuard(), func](
                auto&&... args) mutable
            {
                const auto lock = sharedGuard->lock();
                if (!lock)
                    return;
                dispatch(std::bind(func, std::make_tuple(std::forward<decltype(args)>(args)...)));
            };
    }

    void startTimer(
        int operationId,
        std::chrono::milliseconds timeout,
        OnTimeoutHandler onTimeoutHandler)
    {
        NX_ASSERT(isInSelfAioThread());

        NX_MUTEX_LOCKER lock(&m_mutex);

        auto operationContextIter = m_idToOperationContext.find(operationId);
        if (operationContextIter == m_idToOperationContext.end())
            return; //< The operation has already completed.
        OperationContext& operationContext = operationContextIter->second;

        operationContext.onTimeoutHandler = std::move(onTimeoutHandler);
        operationContext.timer = std::make_unique<Timer>();
        operationContext.timer->start(
            timeout,
            [this, operationId]() { processOperationTimeout(operationId); });
    }

    void processOperationTimeout(int operationId)
    {
        OperationContext operationContext;
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            auto operationContextIter = m_idToOperationContext.find(operationId);
            NX_CRITICAL(operationContextIter != m_idToOperationContext.end());
            operationContext = std::move(operationContextIter->second);
            m_idToOperationContext.erase(operationContextIter);
        }

        operationContext.timer.reset();
        operationContext.onTimeoutHandler();
    }
};

} // namespace nx::network::aio
