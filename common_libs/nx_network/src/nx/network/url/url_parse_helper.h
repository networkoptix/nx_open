#pragma once

#include <string>

#include <QtCore/QString>
#include <QtCore/QUrl>

#include "../socket_common.h"
#include <nx/utils/url.h>

namespace nx {
namespace network {
namespace url {

/**
 * @param scheme Comparison is done in low-case.
 * @return 0 for unknown scheme
 */
NX_NETWORK_API quint16 getDefaultPortForScheme(const QString& scheme);
NX_NETWORK_API SocketAddress getEndpoint(const nx::utils::Url &);
NX_NETWORK_API std::string normalizePath(std::string);
NX_NETWORK_API QString normalizePath(const QString&);

} // namespace url
} // namespace network
} // namespace nx
