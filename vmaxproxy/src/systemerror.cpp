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
        char msgBuf[1024];
        memset( msgBuf, 0, sizeof(msgBuf) );

        FormatMessage(
            FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            errorCode,
            MAKELANGID(LANG_SYSTEM_DEFAULT, SUBLANG_SYS_DEFAULT),
            (LPTSTR) &msgBuf,
            sizeof(msgBuf),
            NULL );
        //удаляем переводы строки
        size_t msgLen = strlen( msgBuf );
        if( msgLen > 0 )
        {
            for( size_t i = msgLen; i > 0; --i )
            {
                if( (msgBuf[i-1] == '\n') || (msgBuf[i-1] == '\r') )
                    msgBuf[i-1] = 0;
                else
                    break;
            }
        }
        return QString::fromLatin1( msgBuf );
#else
        return QString::fromLatin1( strerror( errorCode ) );
#endif
    }

    QString getLastOSErrorText()
    {
        return toString(getLastOSErrorCode());
    }
}
