#pragma once

#include "data.h"
#include "client_info_data.h"

namespace nx {
namespace vms {
namespace api {

/**
 * Parameters of connect request.
 */
struct ConnectionData: Data
{
    QString login;
    QByteArray passwordHash;
    ClientInfoData clientInfo;
};
#define ConnectionData_Fields (login)(passwordHash)(clientInfo)

} // namespace api
} // namespace vms
} // namespace nx
