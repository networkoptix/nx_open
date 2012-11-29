/**********************************************************
* 31 oct 2012
* a.kolesnikov
***********************************************************/

#ifndef SYSTEMERROR_H
#define SYSTEMERROR_H

#ifdef _WIN32
#include <WinSock2.h>
#else
#include <errno.h>
#endif


namespace SystemError
{
#ifdef _WIN32
    typedef DWORD ErrorCode;
#else
    typedef int ErrorCode;
#endif

    enum ErrorCodes
    {
#ifdef _WIN32
        wouldBlock = WSAEWOULDBLOCK
#else
        wouldBlock = EWOULDBLOCK
#endif
    };

    //!Returns error code of previous system call
    ErrorCode getLastOSErrorCode();
    //!Returns text description of \a errorCode
    QString toString( ErrorCode errorCode );
    //!Same as toString(getLastOSErrorCode())
    QString getLastOSErrorText();
}

#endif  //SYSTEMERROR_H
