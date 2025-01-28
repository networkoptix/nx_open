// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "async_handler_executor.h"

#include <nx/utils/coro/task.h>
#include <nx/utils/coro/task_utils.h>
#include <nx/utils/scope_guard.h>

#include "async_handler_executor_detail.h"

namespace nx::utils {

void async_handler_executor_detail::DeferredImpl::submit(MoveOnlyFunc<void()>&& handler)
{
    emit execute({
        shared_from_this(), //< extend lifetime until the slot completes
        std::make_shared<MoveOnlyFunc<void()>>(std::move(handler)),
    });
}

namespace {

class ImmediateImpl:
    public AsyncHandlerExecutor::Impl
{
public:
    void submit(MoveOnlyFunc<void()>&& handler) override
    {
        handler();
    }
};

class ObjectImpl:
    public async_handler_executor_detail::DeferredImpl
{
public:
    explicit ObjectImpl(QObject* object)
    {
        connect(this, &DeferredImpl::execute, object,
            [](auto arg)
            {
                (*arg.handler)();
            },
            Qt::QueuedConnection); //< Avoid resuming in await_suspend when on the same thread.
    }
};

class ThreadImpl:
    public async_handler_executor_detail::DeferredImpl
{
public:
    explicit ThreadImpl(QThread* thread)
    {
        connect(this, &DeferredImpl::execute, this,
            [](auto arg)
            {
                (*arg.handler)();
            },
            Qt::QueuedConnection); //< Avoid resuming in await_suspend when on the same thread.

        moveToThread(thread);
    }
};

} // namespace

AsyncHandlerExecutor::AsyncHandlerExecutor():
    m_impl(std::make_shared<ImmediateImpl>())
{
}

AsyncHandlerExecutor::AsyncHandlerExecutor(QObject* object):
    m_impl(std::make_shared<ObjectImpl>(object))
{
}

AsyncHandlerExecutor::AsyncHandlerExecutor(QThread* thread):
    m_impl(std::make_shared<ThreadImpl>(thread))
{
}

void AsyncHandlerExecutor::submitImpl(MoveOnlyFunc<void()>&& handler) const
{
    m_impl->submit(std::move(handler));
}

bool AsyncHandlerExecutor::Awaiter::await_ready() const noexcept
{
    return dynamic_cast<ImmediateImpl*>(m_impl.get());
}

void AsyncHandlerExecutor::Awaiter::await_suspend(std::coroutine_handle<> h)
{
    auto guard = nx::utils::makeScopeGuard(
        [h, this]() mutable
        {
            m_impl.reset();
            h.resume();
        });

    m_impl->submit(
        [h = std::move(h), guard = std::move(guard)]() mutable
        {
            if (nx::coro::isCancelRequested(h))
            {
                guard.fire();
                return;
            }
            guard.disarm();
            h.resume();
        });
}

void AsyncHandlerExecutor::Awaiter::await_resume() const
{
    if (!m_impl)
        throw nx::coro::TaskCancelException();
}

} // namespace nx::utils
