#pragma once

#include <memory>

#include <openssl/ssl.h>

#include <nx/utils/byte_stream/pipeline.h>

namespace nx::network::ssl {

/**
 * NOTE: Not thread-safe.
 */
class NX_NETWORK_API Pipeline:
    public utils::bstream::Converter
{
public:
    /**
     * @param sslContext Caller may use different contexts on client & server sides.
     */
    Pipeline(SSL_CTX* sslContext);
    ~Pipeline() = default;

    /**
     * NOTE: SSL pipeline does not recover from any I/O error because openssl supports
     * retrying write only with the same data. That does not conform to
     * utils::bstream::AbstractOutput.
     * So, even after wouldBlock write error, next write will always produce non-recoverable
     * failure. Though, it conforms to utils::bstream::AbstractOutput.
     */
    virtual int write(const void* data, size_t size) override;
    virtual int read(void* data, size_t size) override;

    virtual bool eof() const override;
    virtual bool failed() const override;

    bool performHandshake();
    bool isHandshakeCompleted() const;

    bool isReadThirsty() const;
    bool isWriteThirsty() const;

    void shutdown();

protected:
    SSL* ssl();

private:
    enum class State
    {
        init,
        handshakeDone,
    };

    State m_state = State::init;
    std::unique_ptr<SSL, decltype(&SSL_free)> m_ssl;
    bool m_readThirsty = false;
    bool m_writeThirsty = false;
    bool m_eof = false;
    bool m_failed = false;

    void initSslBio(SSL_CTX* sslContext);

    template<typename Func, typename Data>
        int performSslIoOperation(Func sslFunc, Data* data, size_t size);
    int performHandshakeInternal();
    int bioRead(void* buffer, unsigned int bufferLen);
    int bioWrite(const void* buffer, unsigned int bufferLen);

    int handleSslIoResult(int result);
    std::string sslErrorCodeToString(int errorCode);
    void analyzeSslErrorQueue(bool* fatalErrorFound);

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

} // namespace nx::network::ssl
