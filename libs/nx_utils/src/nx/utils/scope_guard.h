// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include "move_only_func.h"

namespace nx::utils {

template<typename Callback>
class ScopeGuard
{
public:
    /** Creates disarmed guard. */
    ScopeGuard() = default;

    /** Creates guard holding @param callback. */
    ScopeGuard(Callback callback):
        m_callback(std::move(callback))
    {
    }

    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;

    ScopeGuard(ScopeGuard&& rhs):
        m_callback(std::exchange(rhs.m_callback, std::nullopt))
    {
    }

    /** Fires this guard and moves @param rhs. */
    ScopeGuard& operator=(ScopeGuard&& rhs)
    {
        fire();
        m_callback = std::exchange(rhs.m_callback, std::nullopt);
        return *this;
    }

    /** Fires this guard. */
    virtual ~ScopeGuard() //noexcept
    {
        fire();
    }

    /** Executes callback and disarms guard. */
    void fire()
    {
        if (m_callback)
        {
            auto callback = std::exchange(m_callback, std::nullopt);
            (*callback)();
        }
    }

    /** Disarms guard, so callback is newer called */
    void disarm()
    {
        m_callback = std::nullopt;
    }

    std::optional<Callback> eject()
    {
        std::optional<Callback> callback;
        std::swap(m_callback, callback);
        return callback;
    }

    explicit operator bool() const
    {
        return static_cast<bool>(m_callback);
    }

private:
    std::optional<Callback> m_callback;
};

using Guard = ScopeGuard<nx::utils::MoveOnlyFunc<void()>>;

template<class Func>
ScopeGuard<Func> makeScopeGuard(Func func)
{
    return ScopeGuard<Func>(std::move(func));
}

//-------------------------------------------------------------------------------------------------

using SharedGuardCallback = nx::utils::MoveOnlyFunc<void()>;
using SharedGuard = ScopeGuard<SharedGuardCallback>;

using SharedGuardPtr = std::shared_ptr<SharedGuard>;

/**
 * @param func is triggered when the last copy of the returned object is destroyed.
 * WARNING: It is not safe to fire shared guard explicitly in a multi-threaded environment.
 * So, to be on safe side, never do it!
 */
template<typename Func>
SharedGuardPtr makeSharedGuard(Func func)
{
    return std::make_shared<SharedGuard>(std::move(func));
}

template<template<typename...> typename Container>
SharedGuardPtr join(Container<SharedGuardPtr> container)
{
    return makeSharedGuard([container = std::move(container)]() mutable { container.clear(); });
}

} // namespace nx::utils
