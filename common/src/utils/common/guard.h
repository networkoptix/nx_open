#ifndef GUARD_H
#define GUARD_H

#include <functional>


/** RAII wrapper for @class std::function<void()> */
template<typename Callback>
class ScopedGuard
{
public:
    /** Creates disarmed guard */
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

    /** Fires this guard and moves @param rhs */
    ScopedGuard& operator=(ScopedGuard&& rhs)
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

    typedef void (ScopedGuard< Callback >::*safe_bool_type)() const;
    void safe_bool_type_retval() const {}

    /** Checks if guard is armed */
    operator safe_bool_type() const
    {
        return m_fired ? 0 : &ScopedGuard< Callback >::safe_bool_type_retval;
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
