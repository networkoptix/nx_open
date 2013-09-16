/**********************************************************
* 10 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#include "mutex.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <pthread.h>
#endif
#include <cstring>


class MutexImpl
{
public:
#ifdef _WIN32
    CRITICAL_SECTION sect;
#else
    pthread_mutex_t mtx;
#endif
};

Mutex::Mutex()
:
    m_impl( new MutexImpl() )
{
#ifdef _WIN32
    memset( &m_impl->sect, 0, sizeof(m_impl->sect) );
    InitializeCriticalSection( &m_impl->sect );
#else
    memset( &m_impl->mtx, 0, sizeof(m_impl->mtx) );
    pthread_mutex_init( &m_impl->mtx, 0 );
#endif
}

Mutex::~Mutex()
{
#ifdef _WIN32
    DeleteCriticalSection( &m_impl->sect );
#else
    pthread_mutex_destroy( &m_impl->mtx );
#endif

    delete m_impl;
    m_impl = NULL;
}

void Mutex::lock()
{
#ifdef _WIN32
    EnterCriticalSection( &m_impl->sect );
#else
    pthread_mutex_lock( &m_impl->mtx );
#endif
}

void Mutex::unlock()
{
#ifdef _WIN32
    LeaveCriticalSection( &m_impl->sect );
#else
    pthread_mutex_unlock( &m_impl->mtx );
#endif
}
