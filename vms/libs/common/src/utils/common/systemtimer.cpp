/**********************************************************
* 15 nov 2012
* a.kolesnikov
***********************************************************/

#include "systemtimer.h"


#ifdef _WIN32

class AbstractSystemTimerImpl
{
public:
    HANDLE hTimer;
    AbstractSystemTimer* timerInstance;

    AbstractSystemTimerImpl( AbstractSystemTimer* _timerInstance )
    :
        hTimer( NULL ),
        timerInstance( _timerInstance )
    {
    }

    void onTimeout()
    {
        timerInstance->onTimeout();
    }
};

static void timerCompletionRoutine(
    LPVOID lpArgToCompletionRoutine,
    DWORD /*dwTimerLowValue*/,
    DWORD /*dwTimerHighValue*/ )
{
    static_cast<AbstractSystemTimerImpl*>(lpArgToCompletionRoutine)->onTimeout();
}

AbstractSystemTimer::AbstractSystemTimer()
:
    m_impl( new AbstractSystemTimerImpl( this ) )
{
    m_impl->hTimer = CreateWaitableTimer( NULL, FALSE, NULL );
}

AbstractSystemTimer::~AbstractSystemTimer()
{
    CloseHandle( m_impl->hTimer );
    m_impl->hTimer = NULL;

    delete m_impl;
    m_impl = NULL;
}

bool AbstractSystemTimer::start( unsigned int timeoutMillis )
{
    LARGE_INTEGER dueTime;
    memset( &dueTime, 0, sizeof(dueTime) );
    dueTime.QuadPart = timeoutMillis;
    dueTime.QuadPart *= -10000;  //100-nanosecond interval, negative value - relative interval
    return SetWaitableTimer(
        m_impl->hTimer,
        &dueTime,
        0,  //single-shot timer
        (PTIMERAPCROUTINE)&timerCompletionRoutine,
        m_impl,
        FALSE );
}

//!Cancel timer. \a onTimeout is not called
bool AbstractSystemTimer::cancel()
{
    return CancelWaitableTimer( m_impl->hTimer );
}


#else

//TODO/IMPL

#endif
