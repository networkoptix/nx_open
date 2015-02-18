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

class QnMutexLocker
{
public:
    QnMutexLocker(
        QnMutex* const mtx,
        const char* sourceFile,
        int sourceLine );
    ~QnMutexLocker();

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

#define SCOPED_MUTEX_LOCK( lockerName, mutexExpr ) \
    QnMutexLocker lockerName( mutexExpr, __FILE__, __LINE__ )

#else   //USE_OWN_MUTEX

#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>

typedef QMutex QnMutex;
typedef QMutexLocker QnMutexLocker;
#define SCOPED_MUTEX_LOCK( lockerName, mutexExpr ) \
    QnMutexLocker lockerName( mutexExpr )

#endif  //USE_OWN_MUTEX

#endif  //NX_MUTEX_H
