#pragma once

#if !defined(Q_MOC_RUN)
    #include <boost/optional.hpp>
#endif

#include <nx/network/async_stoppable.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>

#include "../deprecated/asynchttpclient.h"

namespace nx {
namespace network {
namespace http {

/**
 * Synchronous http client.
 * This is a synchronous wrapper on top of AsyncHttpClient.
 * NOTE: This class is not thread-safe.
 * WARNING: Message body is read asynchronously to some internal buffer.
 */
class NX_NETWORK_API HttpClient:
    public QObject
{
    Q_OBJECT

public:
    HttpClient();
    ~HttpClient();

    void pleaseStop();

    /**
     * @return true if response has been received.
     */
    bool doGet(const nx::utils::Url& url);
    /**
     * @return true if response has been received.
     */
    bool doUpgrade(
        const nx::utils::Url& url,
        const StringType& protocolToUpgradeTo);

    /**
     * @return true if response has been received.
     */
    bool doPost(
        const nx::utils::Url& url,
        const nx::network::http::StringType& contentType,
        nx::network::http::StringType messageBody);

    /**
     * @return true if response has been received.
     */
    bool doPut(
        const nx::utils::Url& url,
        const nx::network::http::StringType& contentType,
        nx::network::http::StringType messageBody);

    /**
     * @return true if response has been received.
     */
    bool doDelete(const nx::utils::Url& url);

    const Response* response() const;
    SystemError::ErrorCode lastSysErrorCode() const;
    bool isValid() const;
    bool eof() const;

    /**
     * Retrieve the currently accumulated message body, removing it from the internal buffer.
     * Blocks while the internal message body buffer is empty. To read the whole message body,
     * should be called in a loop while not eof().
     */
    BufferType fetchMessageBodyBuffer();

    /** Blocks until entire message body is avaliable. */
    boost::optional<BufferType> fetchEntireMessageBody();

    void addAdditionalHeader(const StringType& key, const StringType& value);
    const nx::utils::Url& url() const;
    const nx::utils::Url& contentLocationUrl() const;
    StringType contentType() const;

    /** See AsyncHttpClient::setSubsequentReconnectTries */
    void setSubsequentReconnectTries(int reconnectTries);

    /** See AsyncHttpClient::setTotalReconnectTries */
    void setTotalReconnectTries(int reconnectTries);

    /** See AsyncHttpClient::setSendTimeoutMs */
    void setSendTimeoutMs(unsigned int sendTimeoutMs);

    /** See AsyncHttpClient::setResponseReadTimeoutMs */
    void setResponseReadTimeoutMs(unsigned int messageBodyReadTimeoutMs);

    /** See AsyncHttpClient::setMessageBodyReadTimeoutMs */
    void setMessageBodyReadTimeoutMs(unsigned int messageBodyReadTimeoutMs);

    void setUserAgent(const QString& userAgent);
    void setUserName(const QString& userAgent);
    void setUserPassword(const QString& userAgent);
    void setAuthType(AuthType value);
    void setProxyVia(const SocketAddress& proxyEndpoint);

    void setDisablePrecalculatedAuthorization(bool value);
    void setExpectOnlyMessageBodyWithoutHeaders(bool expectOnlyBody);
    void setIgnoreMutexAnalyzer(bool ignoreMutexAnalyzer);

    const std::unique_ptr<AbstractStreamSocket>& socket();

    /** @return Socket in blocking mode. */
    std::unique_ptr<AbstractStreamSocket> takeSocket();

    /**
     * @param customResponseReadTimeout If not specified, then default timeout is used.
     */
    static bool fetchResource(
        const nx::utils::Url& url,
        BufferType* msgBody,
        StringType* contentType,
        boost::optional<std::chrono::milliseconds> customResponseReadTimeout);

    void setSocket(std::unique_ptr<nx::network::AbstractStreamSocket> socket);
private:
    nx::network::http::AsyncHttpClientPtr m_asyncHttpClient;
    QnWaitCondition m_cond;
    mutable QnMutex m_mutex;
    bool m_done;
    bool m_error;
    bool m_terminated;
    nx::network::http::BufferType m_msgBodyBuffer;
    std::vector<std::pair<StringType, StringType>> m_additionalHeaders;
    boost::optional<int> m_subsequentReconnectTries;
    boost::optional<int> m_reconnectTries;
    boost::optional<unsigned int> m_sendTimeoutMs;
    boost::optional<unsigned int> m_responseReadTimeoutMs;
    boost::optional<unsigned int> m_messageBodyReadTimeoutMs;
    boost::optional<QString> m_userAgent;
    boost::optional<QString> m_userName;
    boost::optional<QString> m_userPassword;
    std::size_t m_maxInternalBufferSize;
    boost::optional<SocketAddress> m_proxyEndpoint;
    boost::optional<AuthType> m_authType;

    bool m_precalculatedAuthorizationDisabled = false;
    bool m_expectOnlyBody = false;
    bool m_ignoreMutexAnalyzer = false;
    std::unique_ptr<nx::network::AbstractStreamSocket> m_socket;

    void instantiateHttpClient();
    template<typename AsyncClientFunc>
        bool doRequest(AsyncClientFunc func);

private slots:
    void onResponseReceived();
    void onSomeMessageBodyAvailable();
    void onDone();
    void onReconnected();
};

} // namespace nx
} // namespace network
} // namespace http
