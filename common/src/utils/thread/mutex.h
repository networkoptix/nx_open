/**********************************************************
* 11 feb 2015
* akolesnikov
***********************************************************/

#ifndef NX_MUTEX_H
#define NX_MUTEX_H

#ifdef USE_OWN_MUTEX

#include <memory>


class QnMutexImpl;

class QnMutex
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

private:
    std::unique_ptr<QnMutexImpl> m_impl;
};

//!This class for internal usage only
class QnMutexLockerBase
{
public:
    QnMutexLockerBase(
        QnMutex* const mtx,
        const char* sourceFile,
        int sourceLine );
    ~QnMutexLockerBase();

    QnMutex* mutex();

    void relock();
    void unlock();

private:
    QnMutex* const m_mtx;
    const char* m_sourceFile;
    int m_sourceLine;
    bool m_locked;
    int m_relockCount;
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
typedef QMutexLocker QnMutexLocker;
typedef QMutexLocker QnMutexLockerBase;

typedef QReadWriteLock QnReadWriteLock;
typedef QReadLocker QnReadLocker;
typedef QWriteLocker QnWriteLocker;

#endif  //USE_OWN_MUTEX

#endif  //NX_MUTEX_H
