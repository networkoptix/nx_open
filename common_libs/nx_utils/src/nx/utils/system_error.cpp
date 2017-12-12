#include "system_error.h"

#ifndef _WIN32
#include <cerrno>
#endif

namespace SystemError {

ErrorCode getLastOSErrorCode()
{
#ifdef _WIN32
    return GetLastError();
#else
    //EAGAIN and EWOULDBLOCK are usually same, but not always
    return errno == EAGAIN ? EWOULDBLOCK : errno;
#endif
}

QString toString(ErrorCode errorCode)
{
#ifdef _WIN32
    wchar_t msgBuf[1024];
    memset(msgBuf, 0, sizeof(msgBuf));

    FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        errorCode,
        MAKELANGID(LANG_SYSTEM_DEFAULT, SUBLANG_SYS_DEFAULT),
        (LPTSTR)&msgBuf,
        sizeof(msgBuf),
        NULL);

    size_t msgLen = wcslen(msgBuf);
    if (msgLen > 0)
    {
        for (size_t i = msgLen; i > 0; --i)
        {
            if ((msgBuf[i - 1] == L'\n') || (msgBuf[i - 1] == L'\r'))
                msgBuf[i - 1] = L' ';
            else
                break;
        }
    }
    return QString::fromWCharArray(msgBuf);
#else
    if (errorCode == dnsServerFailure)
        return QLatin1String("DNS server falure");

    return QString::fromLatin1(strerror(errorCode));
#endif
}

QString getLastOSErrorText()
{
    return toString(getLastOSErrorCode());
}

void setLastErrorCode(ErrorCode errorCode)
{
#ifdef _WIN32
    SetLastError(errorCode);
#else
    errno = errorCode;
#endif
}

} // SystemError
