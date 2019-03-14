#pragma once

#include <QtCore/QString>

class NamedMutexImpl;

//!Mutex that can be used by multiple proceses (QSystemSemaphore does not work well on win32 since it does not release lock on process termination)
/*!
    On win32 it is implemented as Mutex, on Unix - as QSystemSemaphore, since unix provides ability to rollback semaphore state on process termination (SEM_UNDO).
    On win32 QSystemSemaphore is not released with unexpected process termination
*/
class NX_UTILS_API NamedMutex
{
public:
    class ScopedLock
    {
    public:
        ScopedLock( NamedMutex* const mutex )
        :
            m_mutex( mutex ),
            m_locked( false )
        {
            m_locked = m_mutex->lock();
        }
        ~ScopedLock()
        {
            if( m_locked )
                m_mutex->unlock();
        }

        //!Returns true, if mutex is locked. false, otherwise. Error can be read by NamedMutex::getLastError()
        bool locked() const
        {
            return m_locked;
        }

        //!Returns true, if locked mutex
        bool lock()
        {
            m_locked = m_mutex->lock();
            return m_locked;
        }

        //!Returns true, if mutex has been unlocked
        bool unlock()
        {
            m_locked = !m_mutex->unlock();
            return !m_locked;
        }

    private:
        NamedMutex* const m_mutex;
        bool m_locked;
    };

    NamedMutex( const QString& name );
    ~NamedMutex();

    //!Returns true, if mutex has been succesfully opened in constructor
    bool isValid() const;
    /*!
        \return true, if mutex has been locked
    */
    bool lock();
    /*!
        \return true, if mutex has been unlocked
    */
    bool unlock();
    QString errorString() const;

private:
    NamedMutexImpl* m_impl;
};
