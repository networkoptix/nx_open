/**********************************************************
* 31 oct 2012
* a.kolesnikov
***********************************************************/

#ifndef SYSTEMERROR_H
#define SYSTEMERROR_H

#include <QString>

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

#ifdef _WIN32
    static const ErrorCode wouldBlock = WSAEWOULDBLOCK;
    static const ErrorCode inProgress = WSAEWOULDBLOCK;
#else
    static const ErrorCode wouldBlock = EWOULDBLOCK;
    static const ErrorCode inProgress = EINPROGRESS;
#endif

    //!Returns error code of previous system call
    ErrorCode getLastOSErrorCode();
    //!Returns text description of \a errorCode
    QString toString( ErrorCode errorCode );
    //!Same as toString(getLastOSErrorCode())
    QString getLastOSErrorText();
}

#endif  //SYSTEMERROR_H
