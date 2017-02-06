#include "ssl_static_data.h"

#include <memory>
#include <mutex>

#include <openssl/ssl.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/std/cpp14.h>

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

    std::unique_ptr<std::mutex[]> m_openSslGlobalLock;

    OpenSslGlobalLockManager():
        m_initialLockingCallback(CRYPTO_get_locking_callback())
    {
        NX_ASSERT(!m_openSslGlobalLock);
        // not safe here, new can throw exception
        m_openSslGlobalLock.reset(new std::mutex[CRYPTO_num_locks()]);
        CRYPTO_set_locking_callback(&OpenSslGlobalLockManager::openSSLGlobalLock);
    }

    ~OpenSslGlobalLockManager()
    {
        CRYPTO_set_locking_callback(m_initialLockingCallback);
        m_initialLockingCallback = nullptr;
    }

    static void openSSLGlobalLock(int mode, int type, const char* file, int line)
    {
        Q_UNUSED(file);
        Q_UNUSED(line);

        auto& lock = openSslGlobalLockManagerInstance->m_openSslGlobalLock;
        NX_ASSERT(lock);

        if (mode & CRYPTO_LOCK)
            lock.get()[type].lock();
        else
            lock.get()[type].unlock();
    }

private:
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
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    m_serverContext.reset(SSL_CTX_new(SSLv23_server_method()));
    SSL_CTX_set_options(
        m_serverContext.get(),
        SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_SINGLE_DH_USE);

    m_clientContext.reset(SSL_CTX_new(SSLv23_client_method()));
    SSL_CTX_set_options(
        m_clientContext.get(), 0);

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

} // namespace ssl
} // namespace network
} // namespace nx
