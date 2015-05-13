#ifndef GUARD_H
#define GUARD_H

#include <functional>

/** RAII wrapper for @class std::function<void()> */
class Guard
{
public:
    typedef std::function<void()> Callback;

    /** Creates guard holding @param callback */
    explicit Guard(const Callback& callback);
    explicit Guard(Callback&& callback);

    Guard(const Guard&) = delete;
    Guard(Guard&&) = default;

    /** Fires this guard */
    ~Guard();

    /** Executes callback and disarms guard */
    void fire();

    /** Disarms guard, so callback is newer called */
    void disarm();

private:
    Callback m_callback;
};

#endif // GUARD_H
