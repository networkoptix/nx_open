// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

namespace nx::utils {

class NamedMutexImpl;

/**
 * Mutex that can be used by multiple proceses (QSystemSemaphore does not work well on
 * win32 since it does not release lock on process termination)
 * On win32 it is implemented as Mutex, on Unix - as QSystemSemaphore,
 * since unix provides ability to rollback semaphore state on process termination (SEM_UNDO).
 * On win32 QSystemSemaphore is not released with unexpected process termination.
 */
class NX_UTILS_API NamedMutex
{
public:
    class ScopedLock
    {
    public:
        ScopedLock(NamedMutex* const mutex):
            m_mutex(mutex),
            m_locked(false)
        {
            m_locked = m_mutex->lock();
        }
        ~ScopedLock()
        {
            if (m_locked)
                m_mutex->unlock();
        }

        bool locked() const
        {
            return m_locked;
        }

        bool lock()
        {
            m_locked = m_mutex->lock();
            return m_locked;
        }

        bool unlock()
        {
            m_locked = !m_mutex->unlock();
            return !m_locked;
        }

    private:
        NamedMutex* const m_mutex;
        bool m_locked;
    };

    NamedMutex(const QString& name);
    ~NamedMutex();

    /** @return true, if mutex has been succesfully opened in constructor. */
    bool isValid() const;
    bool lock();
    bool unlock();
    QString errorString() const;

private:
    NamedMutexImpl* m_impl;
};

} // namespace nx::utils
