#include "udt_socket.h"

#include <udt/udt.h>

namespace nx {
namespace network {
namespace detail {

SystemError::ErrorCode convertToSystemError(int udtErrorCode)
{
    if (udtErrorCode == CUDTException::SUCCESS)
        return SystemError::noError;
    else if (udtErrorCode == CUDTException::ECONNSETUP)
        return SystemError::connectionReset;
    else if (udtErrorCode == CUDTException::ENOSERVER)
        return SystemError::hostUnreach;
    else if (udtErrorCode == CUDTException::ECONNREJ)
        return SystemError::connectionRefused;
    else if (udtErrorCode == CUDTException::ECONNFAIL)
        return SystemError::connectionAbort;
    else if (udtErrorCode == CUDTException::ECONNLOST)
        return SystemError::connectionReset;
    else if (udtErrorCode == CUDTException::ENOCONN)
        return SystemError::notConnected;
    else if (udtErrorCode == CUDTException::ERESOURCE)
        return SystemError::nomem;
    else if (udtErrorCode == CUDTException::ETHREAD)
        return SystemError::nomem;
    else if (udtErrorCode == CUDTException::ELARGEMSG)
        return SystemError::msgTooLarge;
    else if (udtErrorCode == CUDTException::ENOBUF)
        return SystemError::noBufferSpace;
    else if (udtErrorCode == CUDTException::ERDPERM ||
        udtErrorCode == CUDTException::EWRPERM)
        return SystemError::noPermission;
    else if (udtErrorCode == CUDTException::EINVPARAM)
        return SystemError::invalidData;
    else if (udtErrorCode == CUDTException::EINVSOCK)
        return SystemError::badDescriptor;
    else if (udtErrorCode == CUDTException::ETIMEOUT)
        return SystemError::timedOut;
    else if (udtErrorCode == CUDTException::EINVOP)
        return SystemError::notSupported;
    else if (udtErrorCode == CUDTException::ESOCKFAIL)
        return SystemError::ioError;
    else if (udtErrorCode == CUDTException::ESECFAIL)
        return SystemError::ioError;
    else if (udtErrorCode == CUDTException::EFILE)
        return SystemError::ioError;
    else if (udtErrorCode == CUDTException::EINVRDOFF)
        return SystemError::ioError;
    else if (udtErrorCode == CUDTException::EINVWROFF)
        return SystemError::ioError;
    else if (udtErrorCode == CUDTException::EBOUNDSOCK)
        return SystemError::ioError;
    else if (udtErrorCode == CUDTException::ECONNSOCK)
        return SystemError::ioError;
    else if (udtErrorCode == CUDTException::EUNBOUNDSOCK)
        return SystemError::ioError;
    else if (udtErrorCode == CUDTException::ENOLISTEN)
        return SystemError::ioError;
    else if (udtErrorCode == CUDTException::ERDVNOSERV)
        return SystemError::ioError;
    else if (udtErrorCode == CUDTException::ERDVUNBOUND)
        return SystemError::ioError;
    else if (udtErrorCode == CUDTException::ESTREAMILL)
        return SystemError::ioError;
    else if (udtErrorCode == CUDTException::EDGRAMILL)
        return SystemError::ioError;
    else if (udtErrorCode == CUDTException::EDUPLISTEN)
        return SystemError::ioError;
    else if (udtErrorCode == CUDTException::EINVPOLLID)
        return SystemError::ioError;
    else if (udtErrorCode == CUDTException::EASYNCFAIL)
        return SystemError::ioError;
    else if (udtErrorCode == CUDTException::EASYNCSND)
        return SystemError::wouldBlock;
    else if (udtErrorCode == CUDTException::EASYNCRCV)
        return SystemError::wouldBlock;
    else if (udtErrorCode == CUDTException::EPEERERR)
        return SystemError::ioError;
    else if (udtErrorCode == CUDTException::EUNKNOWN)
        return SystemError::ioError;
    else
        return SystemError::ioError;
}

} // namespace detail
} // namespace network
} // namespace nx
