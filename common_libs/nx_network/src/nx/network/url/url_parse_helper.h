#pragma once

#include <QtCore/QString>
#include <QtCore/QUrl>

#include "../socket_common.h"

namespace nx {
namespace network {
namespace url {

/**
 * @param scheme Comparison is done in low-case.
 * @return 0 for unknown scheme
 */
NX_NETWORK_API quint16 getDefaultPortForScheme(const QString& scheme);
NX_NETWORK_API SocketAddress getEndpoint(const QUrl&);
NX_NETWORK_API QString normalizePath(const QString&);

} // namespace url
} // namespace network
} // namespace nx
