#ifndef NX_CC_COMMON_H
#define NX_CC_COMMON_H

#include "utils/common/systemerror.h"

namespace nx {
namespace cc {

//!Result of mediator request
enum class ResultCode
{
    ok = 0,
    //!requested name could not be resolved
    peerNotFound,
    //!mediator request has timed out
    timedOut,
    //!no connection to the mediator
    notConnected,
    //!requested address is already used
    addrInUse,
    //!local system I/O error
    ioError
};

//!Description of error occured in some cloud connectivity operation
class ErrorDescription
{
public:
    ResultCode resultCode;
    //!System error code (not always available)
    SystemError::ErrorCode sysErrorCode;

    ErrorDescription()
    :
        resultCode( ResultCode::ok ),
        sysErrorCode( SystemError::noError )
    {
    }

    ErrorDescription(
        ResultCode _resultCode,
        SystemError::ErrorCode _sysErrorCode )
    :
        resultCode( _resultCode ),
        sysErrorCode( _sysErrorCode )
    {
    }
};

} // namespace cc
} // namespace nx

#endif  //NX_CC_COMMON_H
