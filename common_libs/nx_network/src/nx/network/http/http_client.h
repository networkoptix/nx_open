#pragma once

#if !defined(Q_MOC_RUN)
    #include <boost/optional.hpp>
#endif

#include <nx/network/async_stoppable.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/thread/mutex.h>

#include "asynchttpclient.h"

namespace nx_http {

/**
 * Synchronous http client.
 * This is a synchronous wrapper on top of AsyncHttpClient.
 * @note This class is not thread-safe.
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
    bool doGet(const QUrl& url);
    /**
     * @return true if response has been received.
     */
    bool doUpgrade(
        const QUrl& url,
        const StringType& protocolToUpgradeTo);

    /**
     * @return true if response has been received.
     */
    bool doPost(
        const QUrl& url,
        const nx_http::StringType& contentType,
        nx_http::StringType messageBody);

    /**
     * @return true if response has been received.
     */
    bool doPut(
        const QUrl& url,
        const nx_http::StringType& contentType,
        nx_http::StringType messageBody);

    /**
     * @return true if response has been received.
     */
    bool doDelete(const QUrl& url);

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

    void addAdditionalHeader(const StringType& key, const StringType& value);
    const QUrl& url() const;
    const QUrl& contentLocationUrl() const;
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

    void setExpectOnlyMessageBodyWithoutHeaders(bool expectOnlyBody);
    void setIgnoreMutexAnalyzer(bool ignoreMutexAnalyzer);

    const std::unique_ptr<AbstractStreamSocket>& socket();

    /** @return Socket in blocking mode. */
    std::unique_ptr<AbstractStreamSocket> takeSocket();

    static bool fetchResource(const QUrl& url, BufferType* msgBody, StringType* contentType);

private:
    nx_http::AsyncHttpClientPtr m_asyncHttpClient;
    QnWaitCondition m_cond;
    mutable QnMutex m_mutex;
    bool m_done;
    bool m_error;
    bool m_terminated;
    nx_http::BufferType m_msgBodyBuffer;
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

    bool m_expectOnlyBody = false;
    bool m_ignoreMutexAnalyzer = false;

    void instantiateHttpClient();
    template<typename AsyncClientFunc>
        bool doRequest(AsyncClientFunc func);

private slots:
    void onResponseReceived();
    void onSomeMessageBodyAvailable();
    void onDone();
    void onReconnected();
};

} // namespace nx_http
