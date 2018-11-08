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
 * Introduced to move any asynchronous operation to aio thread.
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
     * NOTE: Another operation can be started while the previous is still running.
     */
    template<typename Handler, typename... Args>
    void invoke(Handler handler, Args... args)
    {
        invokeWithTimeout(
            std::move(handler),
            std::chrono::milliseconds::zero(),
            OnTimeoutHandler(),
            std::move(args)...);
    }

    template<typename Handler, typename... Args>
    void invokeWithTimeout(
        Handler handler,
        std::chrono::milliseconds timeout,
        OnTimeoutHandler timedOutHandler,
        Args... args)
    {
        // TODO: #ak Move execution to the aio thread and simplify 
        // when perfect forwarding is available.

        const auto operationId = ++m_prevOperationId;

        {
            QnMutexLocker lock(&m_mutex);
            m_idToOperationContext.emplace(operationId, OperationContext{});
        }

        auto processResult = prepareCompletionHandler(
            operationId,
            std::move(handler));

        if constexpr(std::is_member_function_pointer<Func>::value)
        {
            invokeAsMemberFunction(
                std::move(processResult),
                std::move(args)...);
        }
        else
        {
            invokeAsFunctor(
                std::move(processResult),
                std::move(args)...);
        }

        if (timeout > std::chrono::milliseconds::zero())
        {
            post(
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
    QnMutex m_mutex;
    std::map<int, OperationContext> m_idToOperationContext;

    template<typename Handler, typename Class, typename... Args>
    void invokeAsMemberFunction(
        Handler handler,
        Class* pThis,
        Args... args)
    {
        (pThis->*m_func)(
            std::move(args)...,
            std::move(handler));
    }

    template<typename Handler, typename... Args>
    void invokeAsFunctor(Handler handler, Args... args)
    {
        m_func(
            std::move(args)...,
            std::move(handler));
    }

    template<typename Handler>
    auto prepareCompletionHandler(
        int operationId,
        Handler handler)
    {
        storeHandlerToOperationContext(operationId, std::move(handler));

        return
            [this, sharedGuard = m_guard.sharedGuard(), operationId](
                auto... args) mutable
            {
                auto lock = sharedGuard->lock();
                if (!lock)
                    return;

                dispatch(
                    [this, args = std::make_tuple(std::move(args)...),
                        operationId]() mutable
                    {
                        OperationContext context;
                        {
                            QnMutexLocker lock(&m_mutex);
                            auto it = m_idToOperationContext.find(operationId);
                            if (it == m_idToOperationContext.end())
                                return; //< operation has already completed.
                            context = std::move(it->second);
                            m_idToOperationContext.erase(it);
                        }

                        auto handler = context.template takeHandler<Handler>();
                        std::apply(handler, std::move(args));
                    });
            };
    }

    template<typename Handler>
    void storeHandlerToOperationContext(
        int operationId,
        Handler handler)
    {
        QnMutexLocker lock(&m_mutex);
        auto it = m_idToOperationContext.find(operationId);
        NX_CRITICAL(it != m_idToOperationContext.end());
        it->second.saveHandler(std::move(handler));
    }

    void startTimer(
        int operationId,
        std::chrono::milliseconds timeout,
        OnTimeoutHandler onTimeoutHandler)
    {
        NX_ASSERT(isInSelfAioThread());

        QnMutexLocker lock(&m_mutex);

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
            QnMutexLocker lock(&m_mutex);
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
