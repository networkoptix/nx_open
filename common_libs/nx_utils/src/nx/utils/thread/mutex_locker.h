#pragma once

#include <nx/utils/log/assert.h>

namespace nx::utils {

template<typename Mutex>
class NX_UTILS_API Locker
{
public:
    typedef void(Mutex::*LockMethod)(
        const char* /*sourceFile*/, int /*sourceLine*/, int /*lockId*/);

    Locker(Mutex* const mutex, LockMethod lockMethod, const char* sourceFile, int sourceLine):
        m_mutex(mutex),
        m_lockMethod(lockMethod),
        m_sourceFile(sourceFile),
        m_sourceLine(sourceLine)
    {
        (m_mutex->*m_lockMethod)(m_sourceFile, m_sourceLine, m_relockCount);
    }

    ~Locker()
    {
        if (m_isLocked)
            m_mutex->unlock();
    }

    Locker(Locker&& rhs):
        m_mutex(rhs.m_mutex),
        m_lockMethod(rhs.m_lockMethod),
        m_sourceFile(rhs.m_sourceFile),
        m_sourceLine(rhs.m_sourceLine),
        m_relockCount(rhs.m_relockCount),
        m_isLocked(rhs.m_isLocked)
    {
        rhs.m_isLocked = false;
    }

    Locker& operator=(Locker&& rhs)
    {
        if (m_isLocked)
            unlock();

        std::swap(m_mutex, rhs.m_mutex);
        std::swap(m_lockMethod, rhs.m_lockMethod),
        std::swap(m_sourceFile, rhs.m_sourceFile);
        std::swap(m_sourceLine, rhs.m_sourceLine);
        std::swap(m_relockCount, rhs.m_relockCount);
        std::swap(m_isLocked, rhs.m_isLocked);
        return *this;
    }

    Mutex* mutex() const
    {
        return m_mutex;
    }

    void relock()
    {
        NX_ASSERT(!m_isLocked);
        (m_mutex->*m_lockMethod)(m_sourceFile, m_sourceLine, ++m_relockCount);
        m_isLocked = true;
    }

    void unlock()
    {
        m_mutex->unlock();
        m_isLocked = false;
    }

    bool isLocked() const
    {
        return m_isLocked;
    }

private:
    Mutex* m_mutex;
    LockMethod m_lockMethod;
    const char* m_sourceFile;
    int m_sourceLine;
    int m_relockCount = 0;
    bool m_isLocked = true;
};

template<typename Mutex>
class NX_UTILS_API Unlocker
{
public:
    Unlocker(Locker<Mutex>* locker):
        m_locker(locker)
    {
        m_locker->unlock();
    }

    ~Unlocker()
    {
        m_locker->relock();
    }

    Unlocker(Unlocker&& rhs) = delete;
    Unlocker& operator=(Unlocker&& rhs) = delete;

private:
    Locker<Mutex>* m_locker;
};

} // namespace nx::utils
