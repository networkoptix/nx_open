#pragma once

namespace nx {
namespace utils {

template <typename Value>
class Locker
{
public:
    Locker(QnMutex* mutex, Value* value):
        m_mutex(mutex),
        m_value(value)
    {
        m_mutex->lock();
    }

    Locker(Locker&& other)
    {
        m_mutex = other.m_mutex;
        m_value = other.m_value;
        other.m_mutex = nullptr;
        other.m_value = nullptr;
    }

    Locker(const Locker& other) = delete;

    ~Locker()
    {
        if (m_mutex)
            m_mutex->unlock();
    };

    Locker& operator=(const Locker& other) = delete;
    inline Value* operator->() { return m_value; }
    inline const Value* operator->() const { return m_value; }
    inline Locker& operator*() { return *m_value; }
    inline const Locker& operator*() const { return *m_value; }

private:
    mutable QnMutex* m_mutex;
    Value* m_value;
};

template<typename Value>
struct Lockable
{
    Locker<Value> lock()
    {
        return Locker<Value>(&m_mutex, &m_value);
    };

private:
    QnMutex m_mutex;
    Value m_value;
};


} // namespace utils
} // namespace nx
