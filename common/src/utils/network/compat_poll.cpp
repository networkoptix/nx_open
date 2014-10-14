/**********************************************************
* 14 oct 2014
* a.kolesnikov
***********************************************************/

#include "compat_poll.h"


#ifdef _WIN32

static const size_t MS_PER_SEC = 1000;
static const size_t USEC_PER_MS = 1000;

//!Poll implementation based on select call. Useful on winxp
/*!
    \param nfds Number of elements in \a fdarray
    \param timeout If greater than zero - the time, in milliseconds, to wait. Zero - return immediately. Less than zero - wait indefinitely
    \return \n
        - Zero - no sockets were in the queried state before the timer expired
        - Greater than zero - the number of elements in fdarray for which an revents member of the POLLFD structure is nonzero
        - SOCKET_ERROR - an error occurred. Call the WSAGetLastError function to retrieve the extended error code
    \note Implementation is very inefficient! (same as select, actually)
*/
int pollOverSelectWin32(
    pollfd fdarray[],
    ULONG nfds,
    INT timeout )
{
    if( nfds > FD_SETSIZE )
    {
        WSASetLastError( WSAENOBUFS );
        SetLastError( WSAENOBUFS );
        return SOCKET_ERROR;
    }

    //preparing fd sets
    fd_set readfds;
    FD_ZERO( &readfds );
    fd_set writefds;
    FD_ZERO( &writefds );
    fd_set exceptfds;
    FD_ZERO( &exceptfds );
    
    for( ULONG i = 0; i < nfds; ++i )
    {
        if( fdarray[i].events & (POLLRDBAND | POLLRDNORM) )
            FD_SET( fdarray[i].fd, &readfds );
        if( fdarray[i].events & POLLWRNORM )
            FD_SET( fdarray[i].fd, &writefds );
        FD_SET( fdarray[i].fd, &exceptfds );
    }

    struct timeval timeoutVal;
    memset( &timeoutVal, 0, sizeof(timeoutVal) );
    if( timeout > 0 )
    {
        timeoutVal.tv_sec = timeout / MS_PER_SEC;
        timeoutVal.tv_usec = (timeout % MS_PER_SEC) * USEC_PER_MS;
    }

    const int selectResult = select( nfds, &readfds, &writefds, &exceptfds, timeout < 0 ? NULL : &timeoutVal );
    if( selectResult == SOCKET_ERROR || selectResult == 0 )
        return selectResult;

    int pollResult = 0;
    for( ULONG i = 0; i < nfds; ++i )
    {
        if( FD_ISSET( fdarray[i].fd, &readfds ) )
            fdarray[i].revents |= POLLRDNORM;
        if( FD_ISSET( fdarray[i].fd, &writefds ) )
            fdarray[i].revents |= POLLWRNORM;
        if( FD_ISSET( fdarray[i].fd, &exceptfds ) )
        {
            fdarray[i].revents |= POLLERR;

            DWORD errorCode = 0;
            socklen_t optLen = sizeof(errorCode);
            if( getsockopt( fdarray[i].fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&errorCode), &optLen ) == 0 )
            {
                if( errorCode == WSAENOTCONN || errorCode == WSAECONNRESET || errorCode == WSAESHUTDOWN )
                    fdarray[i].revents |= POLLHUP;
            }
        }

        if( fdarray[i].revents )
            ++pollResult;
    }

    return pollResult;
}


class FuncAddressFinder
{
public:
    FuncAddressFinder()
    :
        m_poll( pollOverSelectWin32 )
    {
        HMODULE hLib = LoadLibrary( L"Ws2_32.dll" );
        if( hLib == NULL )
            return;

        PollFuncType wsaPollAddr = (PollFuncType)GetProcAddress( hLib, "WSAPoll" );
        if( wsaPollAddr == NULL )
        {
            FreeLibrary( hLib );
            return;
        }

        m_poll = wsaPollAddr;
        m_loadedLibraries.push_back( hLib );
    }

    ~FuncAddressFinder()
    {
        for( HMODULE hLib: m_loadedLibraries )
            FreeLibrary( hLib );
        m_loadedLibraries.clear();
    }

    PollFuncType getPollFuncAddress() const
    {
        return m_poll;
    }

private:
    PollFuncType m_poll;
    std::list<HMODULE> m_loadedLibraries;
};


PollFuncType getPollFuncAddress()
{
    static FuncAddressFinder funcAddressFinder;
    return funcAddressFinder.getPollFuncAddress();
}

#endif
