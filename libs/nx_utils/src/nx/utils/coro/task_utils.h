// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QVariant>

#include <nx/utils/move_only_func.h>
#include <nx/utils/scope_guard.h>

#include "task.h"

namespace nx::coro {

namespace detail {

class ListenHelper: public QObject
{
    Q_OBJECT

    using base_type = QObject;

public:
    explicit ListenHelper(QObject* parent = nullptr): base_type(parent) {}

    void setCallback(nx::utils::MoveOnlyFunc<void()> callback)
    {
        m_callback = std::move(callback);
    }

public slots:
    void execute() { m_callback(); }

private:
    nx::utils::MoveOnlyFunc<void()> m_callback;
};

} // namespace detail

NX_UTILS_API nx::coro::Task<QVariant> whenProperty(
    QObject* object,
    const char* propertyName,
    std::function<bool(const QVariant&)> condition);

[[nodiscard]] static inline auto guard(QPointer<QObject> object)
{
    return cancelIf([object]() { return !object; });
}

template<typename Duration>
[[nodiscard]] auto qtTimer(Duration duration)
{
    struct Awaiter
    {
        Awaiter(Duration duration): m_duration(duration) {}

        bool await_ready() const { return m_duration.count() <= 0; }

        void await_suspend(std::coroutine_handle<> h)
        {
            auto g = nx::utils::ScopeGuard([this, h] { m_cancelled = true; h.resume(); });
            QTimer::singleShot(
                m_duration,
                [this, h, g = std::move(g)]() mutable
                {
                    m_cancelled = isCancelRequested(h);
                    g.disarm();
                    h.resume();
                });
        }

        void await_resume()
        {
            if (m_cancelled)
                throw nx::coro::TaskCancelException();
        }

        bool m_cancelled = false;
        Duration m_duration;
    };

    return Awaiter(duration);
}

} // namespace nx::coro
