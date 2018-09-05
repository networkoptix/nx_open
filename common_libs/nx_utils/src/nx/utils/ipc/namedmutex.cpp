#include "namedmutex.h"

#ifdef _WIN32
    #include "windows.h"
#endif

#include <QSystemSemaphore>

#ifdef _WIN32
class NamedMutexImpl
{
public:
    QString name;
    HANDLE mutexHandle;
    DWORD prevErrorCode;

    NamedMutexImpl( const QString& _name )
    :
        name( _name ),
        mutexHandle( NULL ),
        prevErrorCode( 0 )
    {
    }
};
#else
class NamedMutexImpl
:
    public QSystemSemaphore
{
public:
    NamedMutexImpl( const QString& name, int initialCount = 0 )
    :
        QSystemSemaphore( name, initialCount )
    {
    }
};
#endif

NamedMutex::NamedMutex( const QString& name )
:
#ifdef _WIN32
    m_impl( new NamedMutexImpl( name ) )
#else
    m_impl( new NamedMutexImpl( name, 1 ) )
#endif
{
#ifdef _WIN32
    m_impl->mutexHandle = CreateMutex( NULL, FALSE, reinterpret_cast<const wchar_t*>(name.utf16()) );
    if( m_impl->mutexHandle == NULL )
        m_impl->prevErrorCode = GetLastError();
#endif
}

NamedMutex::~NamedMutex()
{
#ifdef _WIN32
    if( m_impl->mutexHandle != NULL )
    {
        CloseHandle( m_impl->mutexHandle );
        m_impl->mutexHandle = NULL;
    }
#endif
    delete m_impl;
    m_impl = NULL;
}

bool NamedMutex::isValid() const
{
#ifdef _WIN32
    return m_impl->mutexHandle != NULL;
#else
    return true;
#endif
}

/*!
    \return true, if mutex has been locked
*/
bool NamedMutex::lock()
{
#ifdef _WIN32
    m_impl->prevErrorCode = WaitForSingleObject( m_impl->mutexHandle, INFINITE ) == WAIT_OBJECT_0
        ? 0
        : GetLastError();
    return m_impl->prevErrorCode == 0;
#else
    return m_impl->acquire();
#endif
}

/*!
    \return true, if mutex has been unlocked
*/
bool NamedMutex::unlock()
{
#ifdef _WIN32
    m_impl->prevErrorCode = ReleaseMutex( m_impl->mutexHandle )
        ? 0
        : GetLastError();
    return m_impl->prevErrorCode == 0;
#else
    return m_impl->release();
#endif
}

QString NamedMutex::errorString() const
{
#ifdef _WIN32
    //TODO/IMPL
    return QString();
#else
    return m_impl->errorString();
#endif
}
