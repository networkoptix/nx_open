#pragma once

#include <memory>
#include <atomic>

#include <openssl/ssl.h>

#include "../buffer.h"

namespace nx {
namespace network {
namespace ssl {

/**
 * MUST be called before any SSL usage.
 */
void initOpenSSLGlobalLock();

class SslStaticData
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

private:
    std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> m_serverContext;
    std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> m_clientContext;

    static std::atomic<bool> s_isInitialized;
    static int s_disabledServerVersions;
};

} // namespace ssl
} // namespace network
} // namespace nx
