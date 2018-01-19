#include "ssl_static_data.h"

#ifdef ENABLE_SSL

#include <memory>
#include <mutex>

#include <openssl/ssl.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/unused.h>

#if !defined(SSL_OP_NO_TLSv1_1)
    #define SSL_OP_NO_TLSv1_1 0
#endif

#if !defined(SSL_OP_NO_TLSv1_2)
    #define SSL_OP_NO_TLSv1_2 0
#endif

namespace nx {
namespace network {
namespace ssl {

namespace {

class OpenSslGlobalLockManager;
static std::once_flag kOpenSSLGlobalLockFlag;
static std::unique_ptr<OpenSslGlobalLockManager> openSslGlobalLockManagerInstance;

// SSL global lock. This is a must even if the compilation has configured with THREAD for OpenSSL.
// Based on the documentation of OpenSSL, it internally uses lots of global data structure. Apart
// from this, I have suffered the wired access violation when not give OpenSSL lock callback. The
// documentation says 2 types of callback is needed, however the other one, thread id, is not a
// must since OpenSSL configured with thread support will give default version. Additionally, the
// dynamic lock interface is not used in current OpenSSL version. So we don't use it.
class OpenSslGlobalLockManager
{
public:
    typedef void(*OpenSslLockingCallbackType)(
        int mode, int type, const char *file, int line);

    OpenSslGlobalLockManager():
        m_initialLockingCallback(CRYPTO_get_locking_callback())
    {
        const auto count = (size_t) CRYPTO_num_locks();
        m_mutexList.reserve(count);
        for (size_t i = 0; i < count; ++i)
            m_mutexList.push_back(std::make_unique<QnMutex>());

        CRYPTO_set_locking_callback(&OpenSslGlobalLockManager::manageLock);
    }

    ~OpenSslGlobalLockManager()
    {
        CRYPTO_set_locking_callback(m_initialLockingCallback);
        m_initialLockingCallback = nullptr;
    }

    static void manageLock(int mode, int type, const char* file, int line)
    {
        openSslGlobalLockManagerInstance->manageLockImpl(mode, (size_t) type, file, line);
    }

private:
    void manageLockImpl(int mode, size_t type, const char* file, int line)
    {
        NX_CRITICAL(type < m_mutexList.size());

        auto& mutex = m_mutexList[type];
        if (mode & CRYPTO_LOCK)
        {
            #if defined(USE_OWN_MUTEX)
                mutex->lock(file, line);
            #else
                QN_UNUSED(file, line);
                mutex->lock();
            #endif
        }
        else
        {
            mutex->unlock();
        }
    }

    std::vector<std::unique_ptr<QnMutex>> m_mutexList;
    OpenSslLockingCallbackType m_initialLockingCallback;
};

} // namespace

void initOpenSSLGlobalLock()
{
    std::call_once(
        kOpenSSLGlobalLockFlag,
        []() { openSslGlobalLockManagerInstance.reset(new OpenSslGlobalLockManager()); });
}

//-------------------------------------------------------------------------------------------------
// SslStaticData

static const nx::String kSslSessionId("Network Optix SSL socket");

SslStaticData::SslStaticData():
    pkey(nullptr, &EVP_PKEY_free),
    m_serverContext(nullptr, &SSL_CTX_free),
    m_clientContext(nullptr, &SSL_CTX_free)
{
    s_isInitialized = true;
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    m_serverContext.reset(SSL_CTX_new(SSLv23_server_method()));
    SSL_CTX_set_options(m_serverContext.get(), s_disabledServerVersions | SSL_OP_SINGLE_DH_USE);
    if (s_allowedServerCiphers != "-")
        SSL_CTX_set_cipher_list(m_serverContext.get(), s_allowedServerCiphers.data());

    m_clientContext.reset(SSL_CTX_new(SSLv23_client_method()));
    SSL_CTX_set_options(m_clientContext.get(), 0);

    SSL_CTX_set_session_id_context(
        m_serverContext.get(),
        reinterpret_cast<const unsigned char*>(kSslSessionId.data()),
        kSslSessionId.size());
}

SslStaticData::~SslStaticData()
{
}

SSL_CTX* SslStaticData::serverContext()
{
    return m_serverContext.get();
}

SSL_CTX* SslStaticData::clientContext()
{
    return m_clientContext.get();
}

const nx::String& SslStaticData::sslSessionId()
{
    return kSslSessionId;
}

Q_GLOBAL_STATIC(SslStaticData, SslStaticData_instance);

SslStaticData* SslStaticData::instance()
{
    return SslStaticData_instance();
}

static const int kDisableAllSslVerions =
    SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2;

void SslStaticData::setAllowedServerVersions(const String& versions)
{
    int disabledVersions = kDisableAllSslVerions;
    const auto versionList = versions.split('|');
    NX_LOG(lm("Set server SSL versions: %1").container(versionList), cl_logALWAYS);

    for (const auto& version: versionList)
    {
        const auto s = version.trimmed().toLower();
        if (s == "ssl2" || s == "sslv2")
            disabledVersions ^= SSL_OP_NO_SSLv2;
        else
        if (s == "ssl3" || s == "sslv3")
            disabledVersions ^= SSL_OP_NO_SSLv3;
        else
        if (s == "tls1" || s == "tlsv1")
            disabledVersions ^= SSL_OP_NO_TLSv1;
        else
        if (s == "tls1_1" || s == "tlsv1_1" || s == "tls1.1" || s == "tlsv1.1")
            disabledVersions ^= SSL_OP_NO_TLSv1_1;
        else
        if (s == "tls1_2" || s == "tlsv1_2" || s == "tls1.2" || s == "tlsv1.2")
            disabledVersions ^= SSL_OP_NO_TLSv1_2;
        else
            NX_ASSERT(false, lm("Unknown SSL version: %1").arg(s));
    }

    if (disabledVersions == kDisableAllSslVerions)
        NX_ASSERT(false, "Attempt to disable all SSL versions");
    else
        s_disabledServerVersions = disabledVersions;

    NX_ASSERT(!s_isInitialized, "SSL version does not take effect after first SSL engine usage");
}

void SslStaticData::setAllowedServerCiphers(const String& ciphers)
{
    NX_LOG(lm("Set server SSL ciphers: %1").arg(ciphers), cl_logALWAYS);
    s_allowedServerCiphers = ciphers;
    NX_ASSERT(!s_isInitialized, "SSL ciphers does not take effect after first SSL engine usage");
}

std::atomic<bool> SslStaticData::s_isInitialized(false);
int SslStaticData::s_disabledServerVersions(SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1);
String SslStaticData::s_allowedServerCiphers("HIGH:!RC4:!3DES");

} // namespace ssl
} // namespace network
} // namespace nx

#endif // ENABLE_SSL
