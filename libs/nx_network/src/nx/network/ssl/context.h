// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#ifdef ENABLE_SSL

#include <atomic>
#include <memory>
#include <optional>
#include <regex>
#include <string_view>

#include <openssl/ssl.h>

#include <nx/utils/buffer.h>
#include <nx/utils/thread/mutex.h>

#include "certificate.h"

namespace nx::network::ssl {

/**
 * SSL context.
 * Carries data common for multiple SSL sockets. E.g., SSL certificates, the set of enabled
 * algorithms/ciphers and other things.
 * It is also responsible for providing appropriate certificate depending on the SNI extension.
 * See Context::configureVirtualHost for more details.
 * Different SSL sockets can have different contexts associated. See nx::network::ssl::StreamSocket.
 */
class NX_NETWORK_API Context
{
public:
    Context();
    ~Context();

    /**
     * @return The default server context. It contains the default certificate
     * (if set with setDefaultCertificate).
     */
    std::shared_ptr<SSL_CTX> defaultServerContext();

    std::shared_ptr<SSL_CTX> clientContext();

    const std::string_view& sslSessionId();

    /**
     * Set default certificate for connections where a custom certificate was not specified
     * using configureVirtualHost call.
     * @return true If the certificate was parsed successfully. false if the certificate could not
     * be loaded. Nothing changes in case of an error, the object state is not altered in any way.
     *
     * NOTE: If default certificate is not specified and no certificate set with configureVirtualHost
     * was selected, TLS server rejects connections with SSL_TLSEXT_ERR_NOACK error code.
     *
     * NOTE: Default certificate may be changed. Connections accepted after this call will use the new
     * certificate.
     */
    bool setDefaultCertificate(const std::string& pem, bool allowEcdsaCertificates = false);

    bool setDefaultCertificate(Pem pem, std::string* errorMessage = nullptr);

    Pem getDefaultCertificate() const;

    /**
     * Set certificate to be used for connections with a SNI server name satisfying hostnameRegex.
     * If such virtual host already exists then its certificate is replaced.
     * @return false if the certificate is invalid.
     * NOTE: Hostname regular expressions are sorted and verified in descending length order.
     * NOTE: Virtual host's certificate may be changed. Connections accepted after this call will
     * use the new certificate.
     */
    bool configureVirtualHost(const std::string& hostnameRegex, const std::string& certDataPem);

    /**
     * Removes virtual host previously configured, that matches hostnameRegex.
     * @return False if there is no virtual host to remove, true otherwise.
     */
    bool removeVirtualHost(const std::string& hostnameRegex);

    /**
     * @param versions List of SSL/TLS protocols separated by |.
     * The protocol name has the following format:
     * "tls"|"ssl" [v] major_version "_" minor_version.
     * Example: sslv3|tls1_1|tls1_2.
     * By default, TLS 1.1+ are enabled.
     * @return true if protocol versions have been applied. false if versions is not valid.
     */
    bool setAllowedServerVersions(const std::string& versions);

    /**
     * Set allowed cipher list for TLSv1.2 and below.
     * @param ciphers Passed directly to SSL_CTX_set_cipher_list function.
     * For the format description see SSL_CTX_set_cipher_list docs.
     * @return true if protocol ciphers have been applied. false if not valid.
     */
    bool setAllowedServerCiphers(const std::string& ciphers);

    /**
     * Applies appropriate configuration to SSL connection.
     * This includes but not limited to: enabled server protocols, protocol ciphers.
     * MUST be invoked just after instantiation of SSL type.
     */
    void configure(SSL* ssl);

    /**
     * @return Static instance of the Context. This is used by SSL sockets by default.
     */
    static Context* instance();

private:
    std::shared_ptr<SSL_CTX> createServerContext();

    static int chooseSslContextForIncomingConnectionStatic(SSL* s, int* al, void* arg);
    int chooseSslContextForIncomingConnection(SSL* s, int* al);

    bool bindCertificateToSslContext(
        SSL_CTX* sslContext,
        const std::string& pem);

    bool x509load(SSL_CTX* sslContext, const std::string& pem);
    bool pKeyLoad(SSL_CTX* sslContext, const std::string& pem);

private:
    struct VirtualHostContext
    {
        std::regex hostnameRegex;
        std::shared_ptr<SSL_CTX> sslContext;
    };

    struct HostnameRegexComparator
    {
        bool operator()(const std::string& left, const std::string& right) const
        {
            return left.size() != right.size()
                ? left.size() > right.size()
                : left < right;
        }
    };

    std::shared_ptr<SSL_CTX> m_defaultServerContext;
    std::shared_ptr<SSL_CTX> m_clientContext;
    Pem m_defaultServerPem;
    mutable nx::Mutex m_mutex;
    std::map<
        std::string /*hostname regexp*/, VirtualHostContext,
        HostnameRegexComparator
    > m_virtualHosts;

    std::atomic<int> m_disabledServerVersions = 0;
    std::string m_allowedServerCiphers;
};

} // namespace nx::network::ssl

#endif // ENABLE_SSL
