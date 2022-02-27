// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#ifdef _WIN32
#include <WinSock2.h>
#else
#include <errno.h>
#include <netdb.h>
#endif

#include <string>

namespace SystemError {

#ifdef _WIN32
typedef DWORD ErrorCode;
#else
typedef int ErrorCode;
#endif

static const ErrorCode noError = 0;
static const ErrorCode sslHandshakeError = (ErrorCode) -1;
#ifdef _WIN32
static const ErrorCode wouldBlock = WSAEWOULDBLOCK;
static const ErrorCode inProgress = WSAEWOULDBLOCK;
static const ErrorCode timedOut = WSAETIMEDOUT;
static const ErrorCode fileNotFound = ERROR_FILE_NOT_FOUND;
static const ErrorCode pathNotFound = ERROR_PATH_NOT_FOUND;
static const ErrorCode connectionAbort = WSAECONNABORTED;
static const ErrorCode connectionReset = WSAECONNRESET;
static const ErrorCode connectionRefused = WSAECONNREFUSED;
static const ErrorCode hostNotFound = WSAHOST_NOT_FOUND;
static const ErrorCode notConnected = WSAENOTCONN;
static const ErrorCode interrupted = WSAEINTR;
static const ErrorCode again = EAGAIN;
static const ErrorCode networkUnreachable = WSAENETUNREACH;
static const ErrorCode hostUnreachable = WSAEHOSTUNREACH;
static const ErrorCode noMemory = ERROR_NOT_ENOUGH_MEMORY;
static const ErrorCode notImplemented = ERROR_CALL_NOT_IMPLEMENTED;
static const ErrorCode invalidData = ERROR_INVALID_DATA;
static const ErrorCode addressInUse = WSAEADDRINUSE;
static const ErrorCode noBufferSpace = WSAENOBUFS;
static const ErrorCode noPermission = WSAEACCES;
static const ErrorCode badDescriptor = WSAEBADF;
static const ErrorCode ioError = ERROR_GEN_FAILURE;
static const ErrorCode notSupported = WSAEOPNOTSUPP;
static const ErrorCode messageTooLarge = WSAEMSGSIZE;
static const ErrorCode dnsServerFailure = DNS_ERROR_RCODE_SERVER_FAILURE;
static const ErrorCode already = WSAEALREADY;
static const ErrorCode addressNotAvailable = WSAEADDRNOTAVAIL;
static const ErrorCode unknownProtocolOption = WSAENOPROTOOPT;
#else
static const ErrorCode wouldBlock = EWOULDBLOCK;
static const ErrorCode inProgress = EINPROGRESS;
static const ErrorCode timedOut = ETIMEDOUT;
static const ErrorCode fileNotFound = ENOENT;
static const ErrorCode pathNotFound = ENOENT;
static const ErrorCode connectionAbort = ECONNABORTED;
static const ErrorCode connectionReset = ECONNRESET;
static const ErrorCode connectionRefused = ECONNREFUSED;
static const ErrorCode hostNotFound = EHOSTUNREACH;
static const ErrorCode notConnected = ENOTCONN;
static const ErrorCode interrupted = EINTR;
static const ErrorCode again = EAGAIN;
static const ErrorCode networkUnreachable = ENETUNREACH;
static const ErrorCode hostUnreachable = EHOSTUNREACH;
static const ErrorCode noMemory = ENOMEM;
static const ErrorCode notImplemented = ENOSYS;
static const ErrorCode invalidData = EINVAL;
static const ErrorCode addressInUse = EADDRINUSE;
static const ErrorCode noBufferSpace = ENOBUFS;
static const ErrorCode noPermission = EPERM;
static const ErrorCode badDescriptor = EBADF;
static const ErrorCode ioError = EIO;
static const ErrorCode notSupported = EOPNOTSUPP;
static const ErrorCode messageTooLarge = EMSGSIZE;
static const ErrorCode dnsServerFailure = 0x0F000001;
static const ErrorCode already = EALREADY;
static const ErrorCode addressNotAvailable = EADDRNOTAVAIL;
static const ErrorCode unknownProtocolOption = ENOPROTOOPT;
#endif

/**
 * @return Error code of previous system call.
 */
NX_UTILS_API ErrorCode getLastOSErrorCode();

/**
 * @return text description of errorCode in UTF-8 encoding.
 */
NX_UTILS_API std::string toString(ErrorCode errorCode);

/**
 * Same as toString(getLastOSErrorCode()).
 */
NX_UTILS_API std::string getLastOSErrorText();

NX_UTILS_API void setLastErrorCode(ErrorCode errorCode);

} // SystemError
