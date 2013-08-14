////////////////////////////////////////////////////////////
// 18 aug 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifdef _WIN32

#include "named_pipe_server.h"

#include <aclapi.h>

#include "named_pipe_socket_win32.h"


////////////////////////////////////////////////////////////
//// class NamedPipeServerImpl
////////////////////////////////////////////////////////////
class NamedPipeServerImpl
{
public:
    HANDLE hPipe;

    NamedPipeServerImpl()
    :
        hPipe( INVALID_HANDLE_VALUE )
    {
    }

    bool fillSecurityDescriptor( PSECURITY_DESCRIPTOR pSD )
    {
        bool result = true;

        DWORD dwRes;
        PSID pEveryoneSID = NULL;
        PACL pACL = NULL;
        EXPLICIT_ACCESS ea[1];
        SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;

        // Create a well-known SID for the Everyone group.
        if( !AllocateAndInitializeSid(
                &SIDAuthWorld, 1,
                SECURITY_WORLD_RID,
                0, 0, 0, 0, 0, 0, 0,
                &pEveryoneSID) )
        {
            result = false;
            goto Cleanup;
        }

        // Initialize an EXPLICIT_ACCESS structure for an ACE.
        // The ACE will allow Everyone read access to the key.
        ZeroMemory(ea, sizeof(ea));
        ea[0].grfAccessPermissions = GENERIC_READ | GENERIC_WRITE;
        ea[0].grfAccessMode = SET_ACCESS;
        ea[0].grfInheritance = NO_INHERITANCE;
        ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        ea[0].Trustee.ptstrName  = (LPTSTR) pEveryoneSID;

        // Create a new ACL that contains the new ACEs.
        dwRes = SetEntriesInAcl(1, ea, NULL, &pACL);
        if( ERROR_SUCCESS != dwRes )
        {
            result = false;
            goto Cleanup;
        }

        // Add the ACL to the security descriptor. 
        if( !SetSecurityDescriptorDacl(
                pSD, 
                TRUE,     // bDaclPresent flag   
                pACL, 
                FALSE) )   // not a default DACL 
        {  
            result = false;
            goto Cleanup;
        } 

Cleanup:
        if( pEveryoneSID )
            FreeSid(pEveryoneSID);
        if( pACL )
            LocalFree(pACL);

        return result;
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
    SECURITY_ATTRIBUTES sa;

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
        // The ACE will allow Everyone read access to the key.
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

    const QString win32PipeName = QString::fromLatin1("\\\\.\\pipe\\%1").arg(pipeName);
    m_impl->hPipe = CreateNamedPipe(
        win32PipeName.utf16(),             // pipe name 
        FILE_FLAG_FIRST_PIPE_INSTANCE | PIPE_ACCESS_DUPLEX,       // read/write access 
        PIPE_TYPE_MESSAGE |       // message type pipe 
            PIPE_READMODE_MESSAGE |   // message-read mode 
            PIPE_WAIT,                // blocking mode 
        PIPE_UNLIMITED_INSTANCES, // max. instances  
        NamedPipeSocketImpl::BUFSIZE,      // output buffer size 
        NamedPipeSocketImpl::BUFSIZE,      // input buffer size 
        0,                        // client time-out 
        pSD ? &secAttrs : NULL );                    // default security attribute 

    if( pEveryoneSID )
        FreeSid(pEveryoneSID);
    if( pACL )
        LocalFree(pACL);
    if( pSD )
        LocalFree( pSD );

    return m_impl->hPipe != INVALID_HANDLE_VALUE
        ? SystemError::noError
        : SystemError::getLastOSErrorCode();
}

/*!
    Blocks till new connection is accepted, timeout expires or error occurs
    \param timeoutMillis max time to wait for incoming connection (millis)
    \param sock If return value \a SystemError::noError, \a *sock is assigned with newly-created socket. It must be freed by calling party
*/
SystemError::ErrorCode NamedPipeServer::accept( NamedPipeSocket** sock, int /*timeoutMillis*/ )
{
    //TODO/IMPL: #ak timeoutMillis support

    if( !ConnectNamedPipe( m_impl->hPipe, NULL ) )
        return SystemError::getLastOSErrorCode();

    NamedPipeSocketImpl* sockImpl = new NamedPipeSocketImpl();
    sockImpl->hPipe = m_impl->hPipe;
    sockImpl->onServerSide = true;
    *sock = new NamedPipeSocket( sockImpl );

    return SystemError::noError;
}

#endif
