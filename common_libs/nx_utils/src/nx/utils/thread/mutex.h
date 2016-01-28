/**********************************************************
* 11 feb 2015
* akolesnikov
***********************************************************/

#ifndef NX_MUTEX_H
#define NX_MUTEX_H

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

#else   //USE_OWN_MUTEX

#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>

typedef QMutex QnMutex;
typedef QMutexLocker QnMutexLocker;
typedef QMutexLocker QnMutexLockerBase;

#endif  //USE_OWN_MUTEX

#endif  //NX_MUTEX_H
