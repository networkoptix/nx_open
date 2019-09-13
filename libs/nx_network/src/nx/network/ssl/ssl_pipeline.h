#pragma once

#include <memory>
#include <string>

#include <openssl/ssl.h>

#include <nx/utils/byte_stream/pipeline.h>
#include <nx/utils/move_only_func.h>

namespace nx::network::ssl {

struct Certificate
{
    std::string issuer;
    long serialNumber = 0;

    /**
     * This pointer is valid only inside VerifyCertificateCallback.
     */
    X509* x509Certificate = nullptr;
};

/**
 * @return true if the certificate has been verified. false otherwise.
 */
using VerifyCertificateCallback = nx::utils::MoveOnlyFunc<bool(const Certificate&)>;

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
     * If not empty, ClientHello will contain "servername" attribute.
     */
    void setServerName(const std::string& serverName);

    std::string serverNameFromClientHello() const;

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

    void setVerifyCertificateCallback(VerifyCertificateCallback func);

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
    VerifyCertificateCallback m_verifyCertificateCallback;

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
    static int verifyServerCertificateCallback(int preverify_ok, X509_STORE_CTX* x509_ctx);
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
