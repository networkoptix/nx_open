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

#include <QtCore/QString>


namespace SystemError
{
#ifdef _WIN32
    typedef DWORD ErrorCode;
#else
    typedef int ErrorCode;
#endif

    static const ErrorCode noError = 0;
#ifdef _WIN32
    static const ErrorCode wouldBlock = WSAEWOULDBLOCK;
    static const ErrorCode inProgress = WSAEWOULDBLOCK;
    static const ErrorCode timedOut = WSAETIMEDOUT;
    static const ErrorCode fileNotFound = ERROR_FILE_NOT_FOUND;
    static const ErrorCode interrupted = WSAEINTR;
#else
    static const ErrorCode wouldBlock = EWOULDBLOCK;
    static const ErrorCode inProgress = EINPROGRESS;
    static const ErrorCode timedOut = ETIMEDOUT;
    static const ErrorCode fileNotFound = ENOENT;
    static const ErrorCode interrupted = EINTR;
#endif

    //!Returns error code of previous system call
    ErrorCode getLastOSErrorCode();
    //!Returns text description of \a errorCode
    QString toString( ErrorCode errorCode );
    //!Same as toString(getLastOSErrorCode())
    QString getLastOSErrorText();
}

#endif  //SYSTEMERROR_H
