/**********************************************************
* 12 feb 2015
* akolesnikov
***********************************************************/

#ifdef _DEBUG

#include "mutex_impl.h"


QnMutexImpl::QnMutexImpl( QMutex::RecursionMode mode )
:
    mutex( mode )
{
}

#endif
