// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractEventDispatcher>
#include <QtCore/QPointer>
#include <QtCore/QThread>

#include <nx/utils/scope_guard.h>

#include "task.h"

namespace nx::coro {

static inline bool isCancelRequested(std::coroutine_handle<> h)
{
    const auto request = std::coroutine_handle<TaskBase<>::PromiseBase>::from_address(
        h.address()).promise().m_cancelOnResume;
    return request && request();
}

[[nodiscard]] static inline auto cancelIf(std::function<bool()> condition)
{
    struct suspend_always
    {
        suspend_always(std::function<bool()> condition): m_condition(std::move(condition))
        {
        }

        bool await_ready() const { return false; }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> h)
        {
            auto& promise = std::coroutine_handle<TaskBase<>::PromiseBase>::from_address(
                h.address()).promise();
            promise.addCancelCondition(m_condition);
            return h;
        }

        void await_resume() const
        {
            if (m_condition())
                throw TaskCancelException();
        }

        std::function<bool()> m_condition;
    };

    return suspend_always{std::move(condition)};
}

[[nodiscard]] static inline auto guard(QPointer<QObject> object)
{
    return cancelIf([object]() { return !object; });
}

} // namespace nx::coro
