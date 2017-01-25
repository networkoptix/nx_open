#pragma once

#include <QtCore/QString>
#include <QtCore/QUrl>

#include "../socket_common.h"

namespace nx {
namespace network {
namespace url {

NX_NETWORK_API SocketAddress getEndpoint(const QUrl&);
NX_NETWORK_API QString normalizePath(const QString&);

} // namespace url
} // namespace network
} // namespace nx
