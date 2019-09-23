#include "udt_socket.h"

#include <udt/udt.h>

namespace nx {
namespace network {
namespace detail {

SystemError::ErrorCode convertToSystemError(int udtErrorCode)
{
    if (udtErrorCode == udterror::SUCCESS)
        return SystemError::noError;
    else if (udtErrorCode == udterror::ECONNSETUP)
        return SystemError::connectionReset;
    else if (udtErrorCode == udterror::ENOSERVER)
        return SystemError::hostUnreachable;
    else if (udtErrorCode == udterror::ECONNREJ)
        return SystemError::connectionRefused;
    else if (udtErrorCode == udterror::ECONNFAIL)
        return SystemError::connectionAbort;
    else if (udtErrorCode == udterror::ECONNLOST)
        return SystemError::connectionReset;
    else if (udtErrorCode == udterror::ENOCONN)
        return SystemError::notConnected;
    else if (udtErrorCode == udterror::ERESOURCE)
        return SystemError::noMemory;
    else if (udtErrorCode == udterror::ETHREAD)
        return SystemError::noMemory;
    else if (udtErrorCode == udterror::ELARGEMSG)
        return SystemError::messageTooLarge;
    else if (udtErrorCode == udterror::ENOBUF)
        return SystemError::noBufferSpace;
    else if (udtErrorCode == udterror::ERDPERM ||
        udtErrorCode == udterror::EWRPERM)
        return SystemError::noPermission;
    else if (udtErrorCode == udterror::EINVPARAM)
        return SystemError::invalidData;
    else if (udtErrorCode == udterror::EINVSOCK)
        return SystemError::badDescriptor;
    else if (udtErrorCode == udterror::ETIMEOUT)
        return SystemError::timedOut;
    else if (udtErrorCode == udterror::EINVOP)
        return SystemError::notSupported;
    else if (udtErrorCode == udterror::ESOCKFAIL)
        return SystemError::ioError;
    else if (udtErrorCode == udterror::ESECFAIL)
        return SystemError::ioError;
    else if (udtErrorCode == udterror::EFILE)
        return SystemError::ioError;
    else if (udtErrorCode == udterror::EINVRDOFF)
        return SystemError::ioError;
    else if (udtErrorCode == udterror::EINVWROFF)
        return SystemError::ioError;
    else if (udtErrorCode == udterror::EBOUNDSOCK)
        return SystemError::ioError;
    else if (udtErrorCode == udterror::ECONNSOCK)
        return SystemError::ioError;
    else if (udtErrorCode == udterror::EUNBOUNDSOCK)
        return SystemError::ioError;
    else if (udtErrorCode == udterror::ENOLISTEN)
        return SystemError::ioError;
    else if (udtErrorCode == udterror::ERDVNOSERV)
        return SystemError::ioError;
    else if (udtErrorCode == udterror::ERDVUNBOUND)
        return SystemError::ioError;
    else if (udtErrorCode == udterror::ESTREAMILL)
        return SystemError::ioError;
    else if (udtErrorCode == udterror::EDGRAMILL)
        return SystemError::ioError;
    else if (udtErrorCode == udterror::EDUPLISTEN)
        return SystemError::ioError;
    else if (udtErrorCode == udterror::EINVPOLLID)
        return SystemError::ioError;
    else if (udtErrorCode == udterror::EASYNCFAIL)
        return SystemError::ioError;
    else if (udtErrorCode == udterror::EASYNCSND)
        return SystemError::wouldBlock;
    else if (udtErrorCode == udterror::EASYNCRCV)
        return SystemError::wouldBlock;
    else if (udtErrorCode == udterror::EPEERERR)
        return SystemError::ioError;
    else if (udtErrorCode == udterror::EUNKNOWN)
        return SystemError::ioError;
    else
        return SystemError::ioError;
}

} // namespace detail
} // namespace network
} // namespace nx
