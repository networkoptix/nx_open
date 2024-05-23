// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "context.h"

#ifdef ENABLE_SSL

#include <memory>
#include <mutex>

#include <openssl/ssl.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/string.h>
#include <nx/utils/type_utils.h>

namespace nx::network::ssl {

static constexpr std::string_view kSslSessionId = "Nx network SSL socket";

//-------------------------------------------------------------------------------------------------

namespace {

class OpenSslInitializer
{
public:
    OpenSslInitializer()
    {
        SSL_library_init();
        OpenSSL_add_all_algorithms();
        SSL_load_error_strings();
    }
};

static constexpr int kDefaultDisabledServerVersions =
    SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1;

} // namespace

Context::Context():
    m_disabledServerVersions(kDefaultDisabledServerVersions),
    m_allowedServerCiphers("HIGH:!RC4:!3DES")
{
    static OpenSslInitializer openSslInitializer;

    m_defaultServerContext = createServerContext();

    m_clientContext = std::shared_ptr<SSL_CTX>(
        SSL_CTX_new(SSLv23_client_method()),
        &SSL_CTX_free);
    SSL_CTX_set_options(m_clientContext.get(), 0);
    auto verifyParam = nx::utils::wrapUnique(X509_VERIFY_PARAM_new(), &X509_VERIFY_PARAM_free);
    X509_VERIFY_PARAM_set_flags(verifyParam.get(), X509_V_FLAG_PARTIAL_CHAIN);
    SSL_CTX_set1_param(m_clientContext.get(), verifyParam.get());
}

Context::~Context() = default;

std::shared_ptr<SSL_CTX> Context::defaultServerContext()
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_defaultServerContext;
}

std::shared_ptr<SSL_CTX> Context::clientContext()
{
    return m_clientContext;
}

const std::string_view& Context::sslSessionId()
{
    return kSslSessionId;
}

bool Context::setDefaultCertificate(const std::string& pemStr)
{
    Pem pem;
    if (!pem.parse(pemStr))
        return false;

    return setDefaultCertificate(pem);
}

bool Context::setDefaultCertificate(Pem pem, std::string* errorMessage)
{
    auto newDefaultContext = createServerContext();

    if (!pem.bindToContext(newDefaultContext.get(), errorMessage))
        return false;

    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        m_defaultServerPem = std::move(pem);
        m_defaultServerContext = std::move(newDefaultContext);
    }

    NX_INFO(this, "Default certificate set to %1", m_defaultServerPem);

    return true;
}

Pem Context::getDefaultCertificate() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_defaultServerPem;
}

bool Context::configureVirtualHost(
    const std::string& hostnameRegex,
    const std::string& certDataPem)
{
    auto sslContext = createServerContext();
    if (!bindCertificateToSslContext(sslContext.get(), certDataPem))
        return false;

    NX_MUTEX_LOCKER locker(&m_mutex);

    m_virtualHosts[hostnameRegex] =
        VirtualHostContext{std::regex(hostnameRegex), std::move(sslContext)};

    auto pemToString = [](const std::string& certDataPem)
    {
        X509Certificate x509;
        x509.parsePem(certDataPem);
        return x509.toString();
    };

    NX_INFO(this, "Certificate %1 is loaded for host %2", pemToString(certDataPem), hostnameRegex);

    return true;
}

bool Context::removeVirtualHost(const std::string& hostnameRegex)
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_virtualHosts.erase(hostnameRegex) != 0;
}

// TODO: #akolesnikov Remove this Q_GLOBAL_STATIC.
Q_GLOBAL_STATIC(Context, Context_instance);

Context* Context::instance()
{
    return Context_instance();
}

static constexpr int kDisableAllSslVersions =
    SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
    SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2 | SSL_OP_NO_TLSv1_3;

bool Context::setAllowedServerVersions(const std::string& versionsStr)
{
    int disabledVersions = kDisableAllSslVersions;
    std::vector<std::string> versions;
    nx::utils::split(
        versionsStr, '|',
        [&versions](const auto& token) { versions.push_back(std::string(token)); });

    for (auto& version: versions)
    {
        nx::utils::toLower(&version);
        const auto s = nx::utils::trim(version);

        if (s == "ssl2" || s == "sslv2")
        {
            disabledVersions ^= SSL_OP_NO_SSLv2;
        }
        else if (s == "ssl3" || s == "sslv3")
        {
            disabledVersions ^= SSL_OP_NO_SSLv3;
        }
        else if (s == "tls1" || s == "tlsv1")
        {
            disabledVersions ^= SSL_OP_NO_TLSv1;
        }
        else if (s == "tls1_1" || s == "tlsv1_1" || s == "tls1.1" || s == "tlsv1.1")
        {
            disabledVersions ^= SSL_OP_NO_TLSv1_1;
        }
        else if (s == "tls1_2" || s == "tlsv1_2" || s == "tls1.2" || s == "tlsv1.2")
        {
            disabledVersions ^= SSL_OP_NO_TLSv1_2;
        }
        else if (s == "tls1_3" || s == "tlsv1_3" || s == "tls1.3" || s == "tlsv1.3")
        {
            disabledVersions ^= SSL_OP_NO_TLSv1_3;
        }
        else
        {
            NX_ASSERT(false, "Unknown SSL version: %1", s);
            return false;
        }
    }

    if (disabledVersions == kDisableAllSslVersions)
    {
        NX_ASSERT(false, "Attempt to disable all SSL versions");
        return false;
    }

    NX_INFO(this, "Set server SSL versions: %1", containerString(versions));
    m_disabledServerVersions = disabledVersions;
    return true;
}

bool Context::setAllowedServerCiphers(const std::string& ciphers)
{
    NX_INFO(this, "Set server SSL ciphers: %1", ciphers);

    // TODO: #akolesnikov Validate ciphers.

    NX_MUTEX_LOCKER lock(&m_mutex);
    m_allowedServerCiphers = ciphers;
    return true;
}

void Context::configure(SSL* ssl)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    SSL_set_options(ssl, m_disabledServerVersions.load() | SSL_OP_SINGLE_DH_USE);

    if (m_allowedServerCiphers != "-")
        SSL_set_cipher_list(ssl, m_allowedServerCiphers.data());
    else
        SSL_set_cipher_list(ssl, "DEFAULT");

    SSL_set_session_id_context(
        ssl,
        reinterpret_cast<const unsigned char*>(kSslSessionId.data()),
        kSslSessionId.size());
}

std::shared_ptr<SSL_CTX> Context::createServerContext()
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    auto context = std::shared_ptr<SSL_CTX>(
        SSL_CTX_new(SSLv23_server_method()),
        &SSL_CTX_free);

    SSL_CTX_set_options(context.get(), m_disabledServerVersions | SSL_OP_SINGLE_DH_USE);
    if (m_allowedServerCiphers != "-")
        SSL_CTX_set_cipher_list(context.get(), m_allowedServerCiphers.data());

    SSL_CTX_set_session_id_context(
        context.get(),
        reinterpret_cast<const unsigned char*>(kSslSessionId.data()),
        kSslSessionId.size());

    SSL_CTX_set_tlsext_servername_callback(
        context.get(),
        &Context::chooseSslContextForIncomingConnectionStatic);

    SSL_CTX_set_tlsext_servername_arg(context.get(), this);

    return context;
}

int Context::chooseSslContextForIncomingConnectionStatic(
    SSL* s, int* al, void* arg)
{
    return static_cast<Context*>(arg)->chooseSslContextForIncomingConnection(s, al);
}

int Context::chooseSslContextForIncomingConnection(SSL* s, int* /*al*/)
{
    const int serverNameType = SSL_get_servername_type(s);
    if (serverNameType == -1)
        return SSL_TLSEXT_ERR_OK; //< Using default SSL context.

    const char* serverName = SSL_get_servername(s, serverNameType);
    if (!serverName)
        return SSL_TLSEXT_ERR_OK; //< Using default SSL context.

    NX_MUTEX_LOCKER locker(&m_mutex);

    for (const auto& [hostnameRegex, ctx]: m_virtualHosts)
    {
        if (!std::regex_match(serverName, ctx.hostnameRegex))
            continue;

        SSL_set_SSL_CTX(s, ctx.sslContext.get());
        return SSL_TLSEXT_ERR_OK;
    }

    // Leaving the default SSL context (along with the certificate) in place.
    return SSL_TLSEXT_ERR_OK;
}

bool Context::bindCertificateToSslContext(
    SSL_CTX* sslContext,
    const std::string& pem)
{
    return x509load(sslContext, pem)
        && pKeyLoad(sslContext, pem);
}

bool Context::x509load(
    SSL_CTX* sslContext,
    const std::string& pem)
{
    X509Certificate x509;
    if (!x509.parsePem(pem, SSL_CTX_get_max_cert_list(sslContext)))
    {
        NX_DEBUG(this, "Unable to parse primary X.509 certificate:\n%1", pem);
        return false;
    }

    if (!x509.bindToContext(sslContext))
        return false;

    NX_INFO(this, "X.509 is loaded: %1", x509.toString());

    return true;
}

bool Context::pKeyLoad(SSL_CTX* sslContext, const std::string& certBytes)
{
    auto bio = utils::wrapUnique(
        BIO_new_mem_buf(const_cast<void*>((const void*)certBytes.data()), certBytes.size()),
        &BIO_free);

    PKeyPtr privateKey(PEM_read_bio_PrivateKey(bio.get(), 0, 0, 0), &EVP_PKEY_free);
    if (!privateKey)
    {
        NX_DEBUG(this, "Unable to read PKEY from certificate:\n%1", certBytes);
        return false;
    }

    if (!SSL_CTX_use_PrivateKey(sslContext, privateKey.get()))
    {
        NX_WARNING(this, "Unable to use PKEY");
        return false;
    }

    NX_INFO(this, "PKEY is loaded (SSL init is complete)");
    return true;
}

} // namespace nx::network::ssl

#endif // ENABLE_SSL
