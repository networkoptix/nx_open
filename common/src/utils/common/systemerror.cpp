/**********************************************************
* 31 oct 2012
* a.kolesnikov
***********************************************************/

#include "systemerror.h"

#ifndef _WIN32
#include <cerrno>
#endif


namespace SystemError
{
    ErrorCode getLastOSErrorCode()
    {
#ifdef _WIN32
        return GetLastError();
#else
        return errno;
#endif
    }

    //!Returns text description of \a errorCode
    QString toString( ErrorCode errorCode )
    {
#ifdef _WIN32
        wchar_t msgBuf[1024];
        memset( msgBuf, 0, sizeof(msgBuf) );

        FormatMessage(
            FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            errorCode,
            MAKELANGID(LANG_SYSTEM_DEFAULT, SUBLANG_SYS_DEFAULT),
            (LPTSTR) &msgBuf,
            sizeof(msgBuf),
            NULL );
        
        size_t msgLen = wcslen( msgBuf );
        if( msgLen > 0 )
        {
            for( size_t i = msgLen; i > 0; --i )
            {
                if( (msgBuf[i-1] == L'\n') || (msgBuf[i-1] == L'\r') )
                    msgBuf[i-1] = L' ';
                else
                    break;
            }
        }
        return QString::fromUtf16( msgBuf );
#else
        return QString::fromAscii( strerror( errorCode ) );
#endif
    }

    QString getLastOSErrorText()
    {
        return toString(getLastOSErrorCode());
    }
}
