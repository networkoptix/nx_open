#ifndef GUARD_H
#define GUARD_H

#include <functional>


/** RAII wrapper for @class std::function<void()> */
template<typename Callback>
class ScopedGuard
{
public:
    ScopedGuard()
        : m_fired(true)
    {
    }

    /** Creates guard holding @param callback */
    ScopedGuard(Callback callback)
    :
        m_callback(std::move(callback)),
        m_fired(false)
    {
    }

    ScopedGuard(ScopedGuard&& rhs)
    :
        m_callback(std::move(rhs.m_callback)),
        m_fired(rhs.m_fired)
    {
        rhs.m_fired = true;
    }

    /** Fires this guard */
    ~ScopedGuard() //noexcept
    {
        fire();
    }

    ScopedGuard& operator=(ScopedGuard&& rhs)
    {
        std::swap(m_callback, rhs.m_callback);
        std::swap(m_fired, rhs.m_fired);
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

    operator bool() const
    {
        return !m_fired;
    }

private:
    Callback m_callback;
    bool m_fired;

    ScopedGuard(const ScopedGuard&);
    ScopedGuard& operator=(const ScopedGuard&);
};

typedef ScopedGuard<std::function<void()>> Guard;

template<class Func>
ScopedGuard<Func> makeScopedGuard(Func func)
{
    return ScopedGuard<Func>(std::move(func));
}

#endif // GUARD_H
