#pragma once

namespace nx {
namespace utils {

template<typename Value>
struct Lockable
{
    QnMutex mutex;
    Value value;
};

template <typename Value>
class Locker
{

public:
    Locker(Lockable<Value>* lockable):
        m_lockable(lockable)
    {
        m_lockable->mutex.lock();
    }

    ~Locker()
    {
        m_lockable->mutex.unlock();
    };

private:
    Lockable<Value>* m_lockable;

};

} // namespace utils
} // namespace nx
