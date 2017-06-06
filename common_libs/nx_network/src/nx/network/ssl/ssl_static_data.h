#pragma once

#ifdef ENABLE_SSL

#include <atomic>
#include <memory>

#include <openssl/ssl.h>

#include "../buffer.h"

namespace nx {
namespace network {
namespace ssl {

/**
 * MUST be called before any SSL usage.
 */
void initOpenSSLGlobalLock();

class NX_NETWORK_API SslStaticData
{
public:
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey;

    SslStaticData();
    ~SslStaticData();

    SSL_CTX* serverContext();
    SSL_CTX* clientContext();

    const nx::String& sslSessionId();

    static SslStaticData* instance();
    static void setAllowedServerVersions(const String& versions);
    static void setAllowedServerCiphers(const String& ciphers);

private:
    std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> m_serverContext;
    std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> m_clientContext;

    static std::atomic<bool> s_isInitialized;
    static int s_disabledServerVersions;
    static String s_allowedServerCiphers;
};

} // namespace ssl
} // namespace network
} // namespace nx

#endif // ENABLE_SSL
