/**********************************************************
* 29 aug 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_CC_COMMON_H
#define NX_CC_COMMON_H

#include "utils/common/systemerror.h"


//!cc stands for "cloud connectivity". This namespace contains functionality to establish connection between two servers each behind different NAT
/*!
    TODO #ak doxygen page with description how to use this "cloud connectivity" and how it works
*/
namespace nx_cc
{
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
}

#endif  //NX_CC_COMMON_H
