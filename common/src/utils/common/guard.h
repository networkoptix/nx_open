#ifndef GUARD_H
#define GUARD_H

#include <functional>


/** RAII wrapper for @class std::function<void()> */
template<typename Callback>
class ScopedGuard
{
public:
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
        m_fired(false)
    {
    }

    /** Fires this guard */
    ~ScopedGuard() //noexcept
    {
        fire();
    }

    ScopedGuard& operator=(ScopedGuard&& rhs)
    {
        m_callback = std::move(rhs.m_callback);
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

private:
    Callback m_callback;
    bool m_fired;

    ScopedGuard(const ScopedGuard&);
    ScopedGuard& operator=(const ScopedGuard&);
};

typedef ScopedGuard<std::function<void()>> Guard;

template<class Func>
ScopedGuard<Func> make_scoped_guard(Func func)
{
    return ScopedGuard<Func>(std::move(func));
}

#endif // GUARD_H
