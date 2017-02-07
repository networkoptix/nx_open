#pragma once

#include <memory>

#include <openssl/ssl.h>

#include <QByteArray>

namespace nx {
namespace network {

namespace StreamIoError {

/**
 * Use SystemError::getLastOSErrorCode() to receive error detail.
 */
constexpr int osError = -1;
constexpr int wouldBlock = -2;

} // namespace StreamIoError

class NX_NETWORK_API AbstractOutput
{
public:
    virtual ~AbstractOutput() = default;

    /*
     * @return Bytes written. In case of failure one of StreamIoError* values.
     */
    virtual int write(const void* data, size_t count) = 0;
};

class NX_NETWORK_API AbstractInput
{
public:
    virtual ~AbstractInput() = default;

    /**
     * @return Bytes read. In case of failure one of StreamIoError* values.
     */
    virtual int read(void* data, size_t count) = 0;
};

namespace ssl {

/**
 * @note Not thread-safe.
 */
class NX_NETWORK_API Pipeline:
    public AbstractInput,
    public AbstractOutput
{
public:
    /**
     * @param sslContext Caller may use different contexts on client & server sides.
     */
    Pipeline(SSL_CTX* sslContext);
    virtual ~Pipeline() override;

    void setInput(AbstractInput*);
    void setOutput(AbstractOutput*);

    virtual int write(const void* data, size_t count) override;
    virtual int read(void* data, size_t count) override;

    // TODO: #ak Remove following methods from public API.
    int bioRead(void* buffer, unsigned int bufferLen);
    int bioWrite(const void* buffer, unsigned int bufferLen);
    void close();

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
    AbstractInput* m_inputStream;
    AbstractOutput* m_outputStream;

    void initSslBio(SSL_CTX* sslContext);
    int doHandshake();
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
