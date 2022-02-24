// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "async_handler_executor.h"

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
            });
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
            });

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

} // namespace nx::utils
