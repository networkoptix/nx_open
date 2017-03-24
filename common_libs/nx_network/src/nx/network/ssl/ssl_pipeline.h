#pragma once

#include <memory>

#include <openssl/ssl.h>

#include <QByteArray>

#include <nx/utils/pipeline.h>

namespace nx {
namespace network {
namespace ssl {

/**
 * @note Not thread-safe.
 */
class NX_NETWORK_API Pipeline:
    public utils::pipeline::TwoWayPipeline
{
public:
    /**
     * @param sslContext Caller may use different contexts on client & server sides.
     */
    Pipeline(SSL_CTX* sslContext);
    virtual ~Pipeline() override;

    virtual int write(const void* data, size_t count) override;
    virtual int read(void* data, size_t count) override;

    bool isReadThirsty() const;
    bool isWriteThirsty() const;

    bool eof() const;
    bool failed() const;

    void shutdown();

protected:
    SSL* ssl();

private:
    enum class State
    {
        init,
        handshakeDone,
    };

    State m_state;
    std::unique_ptr<SSL, decltype(&SSL_free)> m_ssl;
    bool m_readThirsty;
    bool m_writeThirsty;
    bool m_eof;
    bool m_failed;

    void initSslBio(SSL_CTX* sslContext);

    template<typename Func, typename Data>
        int performSslIoOperation(Func sslFunc, Data* data, size_t count);
    int doHandshake();
    int bioRead(void* buffer, unsigned int bufferLen);
    int bioWrite(const void* buffer, unsigned int bufferLen);

    int handleSslIoResult(int result);

    static int bioRead(BIO* b, char* out, int outl);
    static int bioWrite(BIO* b, const char* in, int inl);
    static int bioPuts(BIO* bio, const char* str);
    static long bioCtrl(BIO* bio, int cmd, long num, void* /*ptr*/);
    static int bioNew(BIO* bio);
    static int bioFree(BIO* bio);
};

class NX_NETWORK_API ConnectingPipeline:
    public Pipeline
{
public:
    ConnectingPipeline();
};

class NX_NETWORK_API AcceptingPipeline:
    public Pipeline
{
public:
    AcceptingPipeline();
};

} // namespace ssl
} // namespace network
} // namespace nx
