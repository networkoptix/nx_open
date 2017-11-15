#ifndef NX_MUTEX_H
#define NX_MUTEX_H

#include <nx/utils/log/assert.h>

#ifdef USE_OWN_MUTEX

#include <memory>


class QnMutexImpl;

class NX_UTILS_API QnMutex
{
    friend class QnWaitCondition;
    friend class MutexLockAnalyzer;

public:
    enum RecursionMode {
        Recursive,
        NonRecursive
    };

    QnMutex( RecursionMode mode = NonRecursive );
    ~QnMutex();

    void lock(
        const char* sourceFile = 0,
        int sourceLine = 0,
        int lockID = 0 );
    void unlock();
    bool tryLock();
    bool isRecursive() const;

private:
    QnMutexImpl* m_impl;

    QnMutex(const QnMutex&);
    QnMutex& operator=(const QnMutex&);
};

//!This class for internal usage only
class NX_UTILS_API QnMutexLockerBase
{
public:
    QnMutexLockerBase(
        QnMutex* const mtx,
        const char* sourceFile,
        int sourceLine );
    ~QnMutexLockerBase();

    QnMutexLockerBase(QnMutexLockerBase&&);
    QnMutexLockerBase& operator=(QnMutexLockerBase&&);

    QnMutex* mutex();

    void relock();
    void unlock();

    //!introduced for debugging purposes
    bool isLocked() const;

private:
    QnMutex* m_mtx;
    const char* m_sourceFile;
    int m_sourceLine;
    bool m_locked;
    int m_relockCount;

    QnMutexLockerBase(const QnMutexLockerBase&);
    QnMutexLockerBase& operator=(const QnMutexLockerBase&);
};

#define CONCATENATE_DIRECT(s1, s2) s1 ## s2
#define CCAT(s1, s2) CONCATENATE_DIRECT(s1, s2)
#define QnMutexLocker struct CCAT(QnMutexLocker, __LINE__) : public QnMutexLockerBase { CCAT(QnMutexLocker, __LINE__)(QnMutex* mtx) : QnMutexLockerBase( mtx, __FILE__, __LINE__) {} }

class QnReadWriteLock
    : public QnMutex
{
public:
    void lockForWrite() { lock(); }
    void lockForRead() { lock(); }
};

#define QnReadLocker QnMutexLocker
#define QnWriteLocker QnMutexLocker

#else   //USE_OWN_MUTEX

#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QReadWriteLock>

typedef QMutex QnMutex;

/** Adding this class since QMutexLocker does not have move operations */
class QnMutexLocker
{
public:
    QnMutexLocker(QMutex* const mtx)
    :
        m_mutex(mtx),
        m_locked(false)
    {
        relock();
    }
    ~QnMutexLocker()
    {
        if (m_locked)
            unlock();
    }

    QnMutexLocker(QnMutexLocker&& rhs)
    {
        m_mutex = rhs.m_mutex;
        rhs.m_mutex = nullptr;
        m_locked = rhs.m_locked;
        rhs.m_locked = false;
    }
    QnMutexLocker& operator=(QnMutexLocker&& rhs)
    {
        if (this == &rhs)
            return *this;
        if (m_locked)
            unlock();
        m_mutex = rhs.m_mutex;
        rhs.m_mutex = nullptr;
        m_locked = rhs.m_locked;
        rhs.m_locked = false;
        return *this;
    }

    QMutex* mutex()
    {
        return m_mutex;
    }

    void relock()
    {
        NX_ASSERT(!m_locked);
        m_mutex->lock();
        m_locked = true;
    }
    void unlock()
    {
        NX_ASSERT(m_locked);
        m_mutex->unlock();
        m_locked = false;
    }

private:
    QnMutex* m_mutex;
    bool m_locked;
};

typedef QnMutexLocker QnMutexLockerBase;

typedef QReadWriteLock QnReadWriteLock;
typedef QReadLocker QnReadLocker;
typedef QWriteLocker QnWriteLocker;

#endif  //USE_OWN_MUTEX

class QnMutexUnlocker
{
public:
    QnMutexUnlocker(QnMutexLockerBase* const locker):
        m_locker(locker)
    {
        m_locker->unlock();
    }
    ~QnMutexUnlocker()
    {
        m_locker->relock();
    }

private:
    QnMutexLockerBase* const m_locker;
};

#endif  //NX_MUTEX_H
