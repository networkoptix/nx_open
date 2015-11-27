/**********************************************************
* Nov 25, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_NETWORK_UDT_SOCKET_H
#define NX_NETWORK_UDT_SOCKET_H

#include <utils/common/systemerror.h>


namespace detail {

SystemError::ErrorCode convertToSystemError(int udtErrorCode);

}


#endif  //NX_NETWORK_UDT_SOCKET_H
