#pragma once

#include "message.h"
#include "stun_attributes.h"

namespace nx {
namespace network {
namespace stun {

NX_NETWORK_API extern const char* const kUrlSchemeName;
NX_NETWORK_API extern const char* const kSecureUrlSchemeName;

QString NX_NETWORK_API urlSheme(bool isSecure);
bool NX_NETWORK_API isUrlSheme(const QString& scheme);

} // namespace stun
} // namespace network
} // namespace nx
