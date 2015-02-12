/**********************************************************
* 12 feb 2015
* akolesnikov
***********************************************************/

#ifndef NX_MUTEX_IMPL_H
#define NX_MUTEX_IMPL_H

#ifdef _DEBUG

#include <cstdint>

#include <QtCore/QMutex>


class QnMutexImpl
{
public:
    QMutex mutex;
    //!Thread, that have locked mutex is saved here
    std::uintptr_t threadHoldingMutex;

    QnMutexImpl( QMutex::RecursionMode mode );
};

#endif

#endif  //NX_MUTEX_IMPL_H
