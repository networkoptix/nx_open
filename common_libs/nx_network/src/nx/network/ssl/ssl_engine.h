#pragma once

#ifdef ENABLE_SSL

#include <chrono>

#include <openssl/ssl.h>

#include "../buffer.h"

namespace nx {
namespace network {
namespace ssl {

class NX_NETWORK_API Engine
{
    static const size_t kBufferSize;
    static const int kRsaLength;
    static const std::chrono::seconds kCertExpiration;

public:
    static String makeCertificateAndKey(
        const String& name, const String& country, const String& company);

    static bool useCertificateAndPkey(const String& certData);

    static void useOrCreateCertificate(
        const QString& filePath,
        const String& name, const String& country, const String& company);

    static void useRandomCertificate(const String& module);

    static void setAllowedServerVersions(const String& versions);
    static void setAllowedServerCiphers(const String& versions);
};

} // namespace ssl
} // namespace network
} // namespace nx

#endif // ENABLE_SSL
