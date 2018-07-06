#ifndef GUARD_H
#define GUARD_H

#include <functional>
#include <memory>


/** RAII wrapper for @class std::function<void()> */
template<typename Callback>
class ScopeGuard
{
public:
    /** Creates disarmed guard */
    ScopeGuard():
        m_fired(true)
    {
    }

    /** Creates guard holding @param callback */
    ScopeGuard(Callback callback):
        m_callback(std::move(callback)),
        m_fired(false)
    {
    }

    ScopeGuard(ScopeGuard&& rhs):
        m_callback(std::move(rhs.m_callback)),
        m_fired(rhs.m_fired)
    {
        rhs.m_fired = true;
    }

    /** Fires this guard */
    ~ScopeGuard() //noexcept
    {
        fire();
    }

    /** Fires this guard and moves @param rhs */
    ScopeGuard& operator=(ScopeGuard&& rhs)
    {
        fire();
        m_callback = std::move(rhs.m_callback);
        m_fired = rhs.m_fired;

        rhs.m_fired = true;
        return *this;
    }

    /** Executes callback and disarms guard */
    void fire()
    {
        if (!m_fired)
        {
            m_callback();
            m_fired = true;
        }
    }

    /** Disarms guard, so callback is newer called */
    void disarm()
    {
        m_fired = true;
    }

    explicit operator bool() const
    {
        return !m_fired;
    }

private:
    Callback m_callback;
    bool m_fired;

    ScopeGuard(const ScopeGuard&);
    ScopeGuard& operator=(const ScopeGuard&);
};

typedef ScopeGuard<std::function<void()>> Guard;

template<class Func>
ScopeGuard<Func> makeScopeGuard(Func func)
{
    return ScopeGuard<Func>(std::move(func));
}

template<typename Func>
class SharedGuard
{
public:
    SharedGuard(Func func):
        m_func(std::move(func))
    {
    }

    ~SharedGuard()
    {
        m_func();
    }

private:
    Func m_func;
};

/**
 * @param func Will be invoked when last instance of returned value has been removed.
 */
template<typename Func>
std::shared_ptr<SharedGuard<Func>> makeSharedGuard(Func func)
{
    return std::make_shared<SharedGuard<Func>>(std::move(func));
}

#endif // GUARD_H
