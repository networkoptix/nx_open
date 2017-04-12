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
 * @warning Message body is read ascynhronously to some internal buffer.
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
     * Return on receiving response.
     * @return True if response has been received.
     */
    bool doGet(const QUrl& url);

    /**
     * Return on receiving response.
     * @return True if response has been received.
     */
    bool doPost(
        const QUrl& url,
        const nx_http::StringType& contentType,
        nx_http::StringType messageBody);

    /**
     * Return on receiving response.
     * @return True if response has been received.
     */
    bool doPut(
        const QUrl& url,
        const nx_http::StringType& contentType,
        nx_http::StringType messageBody);

    const Response* response() const;
    SystemError::ErrorCode lastSysErrorCode() const;
    bool isValid() const;
    bool eof() const;

    /**
     * Retrieve the currently accumulated message body, removing it from the internal buffer.
     * Block while the internal message body buffer is empty. To read the whole message body,
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
    void setAuthType(AsyncHttpClient::AuthType value);
    void setProxyVia(const SocketAddress& proxyEndpoint);

    const std::unique_ptr<AbstractStreamSocket>& socket();

    /** @return Socket in blocking mode. */
    std::unique_ptr<AbstractStreamSocket> takeSocket();

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
    boost::optional<AsyncHttpClient::AuthType> m_authType;

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
