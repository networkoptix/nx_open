#include "stun_types.h"

namespace nx {
namespace network {
namespace stun {

const char* const kUrlSchemeName = "stun";
const char* const kSecureUrlSchemeName = "stuns";

QString urlSheme(bool isSecure)
{
    return QString::fromUtf8(isSecure ? kSecureUrlSchemeName : kUrlSchemeName);
}

bool isUrlSheme(const QString& scheme)
{
    const auto schemeUtf8 = scheme.toUtf8();
    return schemeUtf8 == kUrlSchemeName || schemeUtf8 == kSecureUrlSchemeName;
}

} // namespace stun
} // namespace network
} // namespace nx
