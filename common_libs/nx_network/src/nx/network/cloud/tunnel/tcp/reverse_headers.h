#pragma once

#include <nx/network/system_socket.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

static const String kUpgrade = "Upgrade";
static const String kConnection = "Connection";
static const String kNxRc = "NXRC/1.0";
static const String kNxRcHostName = "Nxrc-Host-Name";
static const String kNxRcPoolSize = "Nxrc-Pool-Size";
static const String kNxRcKeepAliveOptions = "Nxrc-Keep-Alive-Options";

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
