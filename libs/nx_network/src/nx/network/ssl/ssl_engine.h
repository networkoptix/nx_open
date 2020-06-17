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
public:
    struct CertDetails
    {
        QString commonName;
        QString country;
        QString organization;
    };

    static String makeCertificateAndKey(const CertDetails& details);

    static bool useCertificateAndPkey(const String& certData, const CertDetails& ownDetails = {});
    static bool useOrCreateCertificate(const QString& filePath, const CertDetails& ownDetails = {});
    static bool loadCertificateFromFile(const QString& filePath, const CertDetails& ownDetails = {});
    static void useRandomCertificate(const String& module);

    static void setAllowedServerVersions(const String& versions);
    static void setAllowedServerCiphers(const String& versions);
};

} // namespace ssl
} // namespace network
} // namespace nx

#endif // ENABLE_SSL
