/**********************************************************
* 31 oct 2012
* a.kolesnikov
***********************************************************/

#ifndef SYSTEMERROR_H
#define SYSTEMERROR_H


namespace SystemError
{
#ifdef _WIN32
    typedef DWORD ErrorCode;
#else
    typedef int ErrorCode;
#endif

    //!Returns error code of previous system call
    ErrorCode getLastOSErrorCode();
    //!Returns text description of \a errorCode
    QString toString( ErrorCode errorCode );
}

#endif  //SYSTEMERROR_H
