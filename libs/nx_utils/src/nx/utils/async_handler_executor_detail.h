// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include "async_handler_executor.h"

namespace nx::utils {

class AsyncHandlerExecutor::Impl
{
public:
    virtual void submit(MoveOnlyFunc<void()>&& handler) = 0;

protected:
    virtual ~Impl() = default;
};

namespace async_handler_executor_detail {

class DeferredImpl;

struct ExecuteArg
{
    std::shared_ptr<DeferredImpl> impl; // Keeps impl alive until handler completes.
    std::shared_ptr<MoveOnlyFunc<void()>> handler;
};

class DeferredImpl:
    public QObject,
    public AsyncHandlerExecutor::Impl,
    public std::enable_shared_from_this<DeferredImpl>
{
    Q_OBJECT

public:
    void submit(MoveOnlyFunc<void()>&& handler) override;

signals:
    void execute(
        nx::utils::async_handler_executor_detail::ExecuteArg);
};

} // namespace async_handler_executor_detail

} // namespace nx::utils
