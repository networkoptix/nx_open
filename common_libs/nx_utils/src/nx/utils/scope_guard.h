#pragma once

#include <functional>
#include <memory>
#include <utility>

#include "move_only_func.h"
#include "std/optional.h"

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

    ScopeGuard(Callback creationCallback, Callback destructionCallback):
        m_callback(std::move(destructionCallback))
    {
        if (creationCallback)
            creationCallback();
        else
            NX_ASSERT(false, "Creation callback can't be empty");
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

    explicit operator bool() const
    {
        return static_cast<bool>(m_callback);
    }

private:
    std::optional<Callback> m_callback;
};

using Guard = ScopeGuard<std::function<void()>>;

template<class Func>
ScopeGuard<Func> makeScopeGuard(Func func)
{
    return ScopeGuard<Func>(std::move(func));
}

//-------------------------------------------------------------------------------------------------

using SharedGuard = ScopeGuard<nx::utils::MoveOnlyFunc<void()>>;

using SharedGuardPtr = std::shared_ptr<SharedGuard>;

/**
 * WARNING: It is not safe to fire shared guard explicitely in a multi-threaded environment.
 * So, to be on safe side, never do it!
 */
template<typename Func>
SharedGuardPtr makeSharedGuard(Func func)
{
    return std::make_shared<SharedGuard>(std::move(func));
}

} // namespace nx::utils

Q_DECLARE_METATYPE(nx::utils::SharedGuardPtr)
