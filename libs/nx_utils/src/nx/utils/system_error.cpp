// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_error.h"

#include <array>
#include <locale>

#ifndef _WIN32
#include <cerrno>
#endif

#include <QtCore/QString>

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

std::string toString(ErrorCode errorCode)
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

    // TODO: #akolesnikov Replace with std::codecvt.
    return QString::fromWCharArray(msgBuf).toStdString();
#else
    if (errorCode == dnsServerFailure)
        return "DNS server failure";

    return QString::fromLocal8Bit(strerror(errorCode)).toStdString();
#endif
}

std::string getLastOSErrorText()
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

namespace {
static constexpr std::array<std::pair<SystemError::ErrorCode, std::string_view>, 30>
    kKnownErrorCodes = {{
        {noError, "noError"},
        {sslHandshakeError, "sslHandhshakeError"},
        {wouldBlock, "wouldBlock"},
        {inProgress, "inProgress"},
        {timedOut, "timedOut"},
        {fileNotFound, "fileNotFound"},
        {pathNotFound, "pathNotFound"},
        {connectionAbort, "connectionAbort"},
        {connectionReset, "connectionReset"},
        {connectionRefused, "connectionRefused"},
        {hostNotFound, "hostNotFound"},
        {notConnected, "notConnected"},
        {interrupted, "interrupted"},
        {again, "again"},
        {networkUnreachable, "networkUnreachable"},
        {hostUnreachable, "hostUnreachable"},
        {noMemory, "noMemory"},
        {notImplemented, "notImplemented"},
        {invalidData, "invalidData"},
        {addressInUse, "addressInUse"},
        {noBufferSpace, "noBufferSpace"},
        {noPermission, "noPermission"},
        {badDescriptor, "badDescriptor"},
        {ioError, "ioError"},
        {notSupported, "notSupported"},
        {messageTooLarge, "messageTooLarge"},
        {dnsServerFailure, "dnsServerFailure"},
        {already, "already"},
        {addressNotAvailable, "addressNotAvailable"},
        {unknownProtocolOption, "unknownProtocolOption"},
    }};
} // namespace

std::string toShortString(ErrorCode errorCode)
{
    const auto it = std::find_if(
        kKnownErrorCodes.begin(),
        kKnownErrorCodes.end(),
        [&errorCode](auto& errorPair) { return errorPair.first == errorCode; });

    if (it != kKnownErrorCodes.end())
        return std::string{it->second};

    return "OS_error_" + std::to_string(errorCode);
}

} // namespace SystemError
