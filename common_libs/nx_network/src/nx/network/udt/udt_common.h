/**********************************************************
* Nov 25, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_NETWORK_UDT_SOCKET_H
#define NX_NETWORK_UDT_SOCKET_H

#include <utils/common/systemerror.h>


namespace nx {
namespace network {
namespace detail {

SystemError::ErrorCode convertToSystemError(int udtErrorCode);

}   //detail
}   //network
}   //nx

#endif  //NX_NETWORK_UDT_SOCKET_H
