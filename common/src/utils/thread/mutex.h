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

void saveCurrentFileLineForThread_abrakadabra(
    const char* sourceFile,
    int sourceLine );

//!Use this macro instead of \a QnMutexLockerInternal
#define QnMutexLocker saveCurrentFileLineForThread_abrakadabra( __FILE__, __LINE__ ); QnMutexLockerInternal

//!This class for internal usage only
class QnMutexLockerInternal
{
public:
    //!This initializer takes file/line saved by \a saveCurrentFileLine
    QnMutexLockerInternal( QnMutex* const mtx );
    QnMutexLockerInternal(
        QnMutex* const mtx,
        const char* sourceFile,
        int sourceLine );
    ~QnMutexLockerInternal();

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

#else   //USE_OWN_MUTEX

#include <QtCore/QMutex>
#include <QtCore/QnMutexLocker>

typedef QMutex QnMutex;
typedef QnMutexLocker QnMutexLocker;
#define QnMutexLocker lockerName( mutexExpr ) \
    QnMutexLocker lockerName( mutexExpr )

#endif  //USE_OWN_MUTEX

#endif  //NX_MUTEX_H
