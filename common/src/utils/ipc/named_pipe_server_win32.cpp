////////////////////////////////////////////////////////////
// 18 aug 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifdef _WIN32

#include "named_pipe_server.h"

#include <aclapi.h>

#include "named_pipe_socket_win32.h"

#pragma comment(lib, "advapi32.lib")


////////////////////////////////////////////////////////////
//// class NamedPipeServerImpl
////////////////////////////////////////////////////////////
class NamedPipeServerImpl
{
public:
    HANDLE hPipe;
    HANDLE overlappedIOEvent;
    struct _OVERLAPPED overlappedInfo;

    NamedPipeServerImpl()
    :
        hPipe( INVALID_HANDLE_VALUE ),
        overlappedIOEvent( NULL )
    {
        memset( &overlappedInfo, 0, sizeof(overlappedInfo) );
    }
};

////////////////////////////////////////////////////////////
//// class NamedPipeServer
////////////////////////////////////////////////////////////
NamedPipeServer::NamedPipeServer()
:
    m_impl( new NamedPipeServerImpl() )
{
}

NamedPipeServer::~NamedPipeServer()
{
    CloseHandle( m_impl->hPipe );
    m_impl->hPipe = INVALID_HANDLE_VALUE;

    CloseHandle( m_impl->overlappedIOEvent );
    m_impl->overlappedIOEvent = NULL;

    delete m_impl;
    m_impl = NULL;
}

//!Openes \a pipeName and listenes it for connections
/*!
    This method returns immediately
*/
SystemError::ErrorCode NamedPipeServer::listen( const QString& pipeName )
{
    DWORD dwRes;
    PSID pEveryoneSID = NULL;
    PACL pACL = NULL;
    EXPLICIT_ACCESS ea[1];
    SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;

    PSECURITY_DESCRIPTOR pSD = NULL;
    for( ;; )   //for break
    {
        // Create a well-known SID for the Everyone group.
        if( !AllocateAndInitializeSid(
                &SIDAuthWorld, 1,
                SECURITY_WORLD_RID,
                0, 0, 0, 0, 0, 0, 0,
                &pEveryoneSID) )
        {
            break;
        }

        // Initialize an EXPLICIT_ACCESS structure for an ACE.
        // The ACE will allow Everyone read & write access to the pipe
        ZeroMemory(ea, sizeof(ea));
        ea[0].grfAccessPermissions = GENERIC_READ | GENERIC_WRITE;
        ea[0].grfAccessMode = SET_ACCESS;
        ea[0].grfInheritance = NO_INHERITANCE;
        ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        ea[0].Trustee.ptstrName  = (LPTSTR) pEveryoneSID;

        // Create a new ACL that contains the new ACEs.
        dwRes = SetEntriesInAcl(1, ea, NULL, &pACL);
        if( dwRes != ERROR_SUCCESS )
            break;

        pSD = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH); 
        if( !pSD )
            break;
        if( !InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION)
            || !SetSecurityDescriptorDacl( pSD, TRUE, pACL, FALSE ) )
        {
            LocalFree( pSD );
            pSD = NULL;
            break;
        }

        break;
    }

    struct _SECURITY_ATTRIBUTES secAttrs;
    memset( &secAttrs, 0, sizeof(secAttrs) );
    secAttrs.nLength = sizeof(SECURITY_ATTRIBUTES);
    secAttrs.lpSecurityDescriptor = pSD;
    secAttrs.bInheritHandle = FALSE;

    const QString win32PipeName = lit("\\\\.\\pipe\\%1").arg(pipeName);
    m_impl->hPipe = CreateNamedPipe(
        reinterpret_cast<const wchar_t *>(win32PipeName.utf16()),              // pipe name 
        FILE_FLAG_FIRST_PIPE_INSTANCE | PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,       // read/write access, overlapped I/O
        PIPE_TYPE_MESSAGE |                 // message type pipe 
            PIPE_READMODE_MESSAGE |         // message-read mode 
            PIPE_WAIT,                      // blocking mode 
        PIPE_UNLIMITED_INSTANCES,           // max. instances  
        NamedPipeSocketImpl::BUFSIZE,       // output buffer size 
        NamedPipeSocketImpl::BUFSIZE,       // input buffer size 
        0,                                  // client time-out 
        pSD ? &secAttrs : NULL );

    if( pEveryoneSID )
        FreeSid(pEveryoneSID);
    if( pACL )
        LocalFree(pACL);
    if( pSD )
        LocalFree( pSD );

    if( m_impl->hPipe == INVALID_HANDLE_VALUE )
        return SystemError::getLastOSErrorCode();

    //creating event for overlapped I/O
    m_impl->overlappedIOEvent = CreateEvent(
        NULL,
        TRUE,
        FALSE,
        NULL );
    if( m_impl->overlappedIOEvent == NULL )
    {
        const SystemError::ErrorCode osErrorCode = SystemError::getLastOSErrorCode();
        CloseHandle( m_impl->hPipe );
        m_impl->hPipe = INVALID_HANDLE_VALUE;
        return osErrorCode;
    }

    return SystemError::noError;
}

/*!
    Blocks till new connection is accepted, timeout expires or error occurs
    \param timeoutMillis max time to wait for incoming connection (millis)
    \param sock If return value \a SystemError::noError, \a *sock is assigned with newly-created socket. It must be freed by calling party
*/
SystemError::ErrorCode NamedPipeServer::accept( NamedPipeSocket** sock, int timeoutMillis )
{
    memset( &m_impl->overlappedInfo, 0, sizeof(m_impl->overlappedInfo) );
    m_impl->overlappedInfo.hEvent = m_impl->overlappedIOEvent;
    ResetEvent( m_impl->overlappedInfo.hEvent );

    if( !ConnectNamedPipe( m_impl->hPipe, &m_impl->overlappedInfo ) )
    {
        switch( GetLastError() )
        {
            case ERROR_PIPE_CONNECTED:
                //connection has already been established
                break;

            case ERROR_IO_PENDING:
            {
                //waiting for new connection
                switch( WaitForSingleObject(
                            m_impl->overlappedInfo.hEvent,
                            timeoutMillis == -1 ? INFINITE : timeoutMillis ) )
                {
                    case WAIT_OBJECT_0:
                    {
                        DWORD numberOfBytesTransferred = 0;
                        if( !GetOverlappedResult( m_impl->hPipe, &m_impl->overlappedInfo, &numberOfBytesTransferred, FALSE ) )
                            return SystemError::getLastOSErrorCode();
                        break;
                    }

                    case WAIT_TIMEOUT:
                        CancelIo( m_impl->hPipe );
                        return SystemError::timedOut;

                    case WAIT_FAILED:
                    default:
                        CancelIo( m_impl->hPipe );
                        return SystemError::getLastOSErrorCode();
                }
                break;
            }

            default:
                return SystemError::getLastOSErrorCode();
        }
    }

    NamedPipeSocketImpl* sockImpl = new NamedPipeSocketImpl();
    sockImpl->hPipe = m_impl->hPipe;
    sockImpl->onServerSide = true;
    *sock = new NamedPipeSocket( sockImpl );

    return SystemError::noError;
}

#endif
