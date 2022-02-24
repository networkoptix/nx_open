// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "named_pipe_server.h"

#include <aclapi.h>

#include <QtCore/QString>

#include "named_pipe_socket_win.h"
#include "../string.h"

#pragma comment(lib, "advapi32.lib")

namespace nx::utils {

//-------------------------------------------------------------------------------------------------
// class NamedPipeServerImpl.

class NamedPipeServerImpl
{
public:
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    HANDLE overlappedIOEvent = NULL;
    struct _OVERLAPPED overlappedInfo;

    NamedPipeServerImpl()
    {
        memset(&overlappedInfo, 0, sizeof(overlappedInfo));
    }
};

//-------------------------------------------------------------------------------------------------
// class NamedPipeServer.

NamedPipeServer::NamedPipeServer():
    m_impl(new NamedPipeServerImpl())
{
}

NamedPipeServer::~NamedPipeServer()
{
    CloseHandle(m_impl->hPipe);
    m_impl->hPipe = INVALID_HANDLE_VALUE;

    CloseHandle(m_impl->overlappedIOEvent);
    m_impl->overlappedIOEvent = NULL;

    delete m_impl;
    m_impl = NULL;
}

SystemError::ErrorCode NamedPipeServer::listen(const std::string& pipeName)
{
    DWORD dwRes;
    PSID pEveryoneSID = NULL;
    PACL pACL = NULL;
    EXPLICIT_ACCESS ea[1];
    SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;

    PSECURITY_DESCRIPTOR pSD = NULL;
    for (;; )   //for break
    {
        // Make a well-known SID for the group "Everybody".
        if (!AllocateAndInitializeSid(
            &SIDAuthWorld, 1,
            SECURITY_WORLD_RID,
            0, 0, 0, 0, 0, 0, 0,
            &pEveryoneSID))
        {
            break;
        }

        // Fill the EXPLICIT_ACCESS structure for an ACE, allowing read/write access for the group
        // "Everybody".
        ZeroMemory(ea, sizeof(ea));
        ea[0].grfAccessPermissions = GENERIC_READ | GENERIC_WRITE;
        ea[0].grfAccessMode = SET_ACCESS;
        ea[0].grfInheritance = NO_INHERITANCE;
        ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        ea[0].Trustee.ptstrName = (LPTSTR)pEveryoneSID;

        // Make a new ACL containing the ACE defined by the above EXPLICIT_ACCESS instance.
        dwRes = SetEntriesInAcl(1, ea, NULL, &pACL);
        if (dwRes != ERROR_SUCCESS)
            break;

        pSD = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
        if (!pSD)
            break;
        if (!InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION)
            || !SetSecurityDescriptorDacl(pSD, TRUE, pACL, FALSE))
        {
            LocalFree(pSD);
            pSD = NULL;
            break;
        }

        break;
    }

    struct _SECURITY_ATTRIBUTES secAttrs;
    memset(&secAttrs, 0, sizeof(secAttrs));
    secAttrs.nLength = sizeof(SECURITY_ATTRIBUTES);
    secAttrs.lpSecurityDescriptor = pSD;
    secAttrs.bInheritHandle = FALSE;

    // TODO: #akolesnikov remove QString
    const QString win32PipeName = QString("\\\\.\\pipe\\%1").arg(pipeName.c_str());
    m_impl->hPipe = CreateNamedPipe(
        reinterpret_cast<const wchar_t*>(win32PipeName.utf16()),              // pipe name
        FILE_FLAG_FIRST_PIPE_INSTANCE | PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,       // read/write access, overlapped I/O
        PIPE_TYPE_MESSAGE |                 // message type pipe
        PIPE_READMODE_MESSAGE |         // message-read mode
        PIPE_WAIT,                      // blocking mode
        PIPE_UNLIMITED_INSTANCES,           // max. instances
        NamedPipeSocketImpl::BUFSIZE,       // output buffer size
        NamedPipeSocketImpl::BUFSIZE,       // input buffer size
        0,                                  // client time-out
        pSD ? &secAttrs : NULL);

    if (pEveryoneSID)
        FreeSid(pEveryoneSID);
    if (pACL)
        LocalFree(pACL);
    if (pSD)
        LocalFree(pSD);

    if (m_impl->hPipe == INVALID_HANDLE_VALUE)
        return SystemError::getLastOSErrorCode();

    //creating event for overlapped I/O
    m_impl->overlappedIOEvent = CreateEvent(
        NULL,
        TRUE,
        FALSE,
        NULL);
    if (m_impl->overlappedIOEvent == NULL)
    {
        const SystemError::ErrorCode osErrorCode = SystemError::getLastOSErrorCode();
        CloseHandle(m_impl->hPipe);
        m_impl->hPipe = INVALID_HANDLE_VALUE;
        return osErrorCode;
    }

    return SystemError::noError;
}

SystemError::ErrorCode NamedPipeServer::accept(NamedPipeSocket** sock, int timeoutMillis)
{
    memset(&m_impl->overlappedInfo, 0, sizeof(m_impl->overlappedInfo));
    m_impl->overlappedInfo.hEvent = m_impl->overlappedIOEvent;
    ResetEvent(m_impl->overlappedInfo.hEvent);

    if (!ConnectNamedPipe(m_impl->hPipe, &m_impl->overlappedInfo))
    {
        switch (GetLastError())
        {
            case ERROR_PIPE_CONNECTED:
                //connection has already been established
                break;

            case ERROR_IO_PENDING:
            {
                //waiting for new connection
                switch (WaitForSingleObject(
                    m_impl->overlappedInfo.hEvent,
                    timeoutMillis == -1 ? INFINITE : timeoutMillis))
                {
                    case WAIT_OBJECT_0:
                    {
                        DWORD numberOfBytesTransferred = 0;
                        if (!GetOverlappedResult(m_impl->hPipe, &m_impl->overlappedInfo, &numberOfBytesTransferred, FALSE))
                            return SystemError::getLastOSErrorCode();
                        break;
                    }

                    case WAIT_TIMEOUT:
                        CancelIo(m_impl->hPipe);
                        return SystemError::timedOut;

                    case WAIT_FAILED:
                    default:
                        CancelIo(m_impl->hPipe);
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
    *sock = new NamedPipeSocket(sockImpl);

    return SystemError::noError;
}

} // namespace nx::utils
