#pragma once

#include <map>
#include <memory>
#include <tuple>

#include "basic_pollable.h"

namespace nx::network::aio {

/**
 * Helps to manage multiple concurrent asynchronous operations performed by some executor.
 */
template<typename Executor>
// requires std::is_base_of<nx::network::aio::BasicPollable, Executor>
class AsyncOperationPool:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

public:
    struct Context
    {
        std::unique_ptr<Executor> executor;
        nx::utils::MoveOnlyFunc<void(std::unique_ptr<Executor>)> handler;
    };

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);

        for (auto& request: m_requests)
            request.second->executor->bindToAioThread(aioThread);
    }

    template<typename Handler, typename... Args>
    Context& add(
        std::unique_ptr<Executor> executor,
        Handler&& handler,
        Args&&... args)
    {
        NX_ASSERT(executor->getAioThread() == getAioThread());

        auto context = std::make_unique<Context>();
        context->executor = std::move(executor);
        context->handler =
            [handler = std::forward<Handler>(handler),
                args = std::make_tuple(std::forward<Args>(args)...)](
                    std::unique_ptr<Executor> executor) mutable
            {
                auto allArgs = std::tuple_cat(
                    std::make_tuple(std::move(executor)),
                    std::move(args));
                std::apply(handler, std::move(allArgs));
            };
        return *m_requests.emplace(context.get(), std::move(context)).first->first;
    }

    void completeRequest(Context* contextPtr)
    {
        auto it = m_requests.find(contextPtr);
        NX_ASSERT(it != m_requests.end());
        if (it == m_requests.end())
            return;

        auto context = std::move(it->second);
        m_requests.erase(it);

        context->handler(std::move(context->executor));
    }

protected:
    virtual void stopWhileInAioThread() override
    {
        m_requests.clear();
    }

private:
    std::map<Context*, std::unique_ptr<Context>> m_requests;
};

} // namespace nx::network::aio
