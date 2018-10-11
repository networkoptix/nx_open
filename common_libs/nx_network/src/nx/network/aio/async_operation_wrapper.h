#pragma once

#include <memory>
#include <tuple>
#include <type_traits>

#include <nx/utils/async_operation_guard.h>

#include "basic_pollable.h"

namespace nx::network::aio {

/**
 * Introduced to move any asynchronous operation to aio thread.
 * That means:
 * 1. Completion handler is called within aio thread
 * 2. Operation can be safely cancelled using API of nx::network::aio::BasicPollable.
 * Cancelled operation will still be running, but its completion is ignored
 * and does not access freed memory.
 */
template<typename Func>
class AsyncOperationWrapper:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

public:
    template<typename _Func>
    AsyncOperationWrapper(_Func func):
        m_func(std::move(func))
    {
    }

    virtual ~AsyncOperationWrapper()
    {
        m_guard.reset();
    }

    template<typename Handler, typename... Args>
    void invoke(Handler handler, Args... args)
    {
        auto processResult = 
            [this, sharedGuard = m_guard.sharedGuard(),
                handler = std::move(handler)](auto... args) mutable
            {
                if (auto lock = sharedGuard->lock())
                {
                    dispatch(
                        [args = std::make_tuple(std::move(args)...),
                            handler = std::move(handler)]() mutable
                        {
                            std::apply(handler, std::move(args));
                        });
                }
            };

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
    }

protected:
    virtual void stopWhileInAioThread() override
    {
        base_type::stopWhileInAioThread();

        m_guard.reset();
    }

private:
    Func m_func;
    nx::utils::AsyncOperationGuard m_guard;

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
};

} // namespace nx::network::aio
