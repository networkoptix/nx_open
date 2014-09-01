/**********************************************************
* 29 aug 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_CC_COMMON_H
#define NX_CC_COMMON_H

#include "utils/common/systemerror.h"


//!cc stands for "cloud connectivity". This namespace contains functionality to establish connection between two servers each behind different NAT
namespace nx_cc
{
    class ErrorDescription
    {
    public:
        SystemError::ErrorCode sysErrorCode;

        ErrorDescription()
        :
            sysErrorCode( SystemError::noError )
        {
        }
    };
}

#endif  //NX_CC_COMMON_H
