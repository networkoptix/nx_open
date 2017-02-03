/**********************************************************
* 26 nov 2012
* a.kolesnikov
***********************************************************/

#pragma once

#ifndef Q_MOC_RUN
#include <boost/optional.hpp>
#endif

#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/thread/mutex.h>

#include <utils/common/stoppable.h>

#include "asynchttpclient.h"


namespace nx_http {

/**
 * Synchronous http client.
 * This is a synchronous wrapper on top of AsyncHttpClient.
 * @note This class is not thread-safe.
 * @warning Message body is read ascynhronously to some internal buffer.
 */
class NX_NETWORK_API HttpClient:
    public QObject,
    public QnStoppable
{
    Q_OBJECT

public:
    HttpClient();
    ~HttpClient();

    virtual void pleaseStop() override;

    /**
        Returns on receiving response
        @return \a true if response has been received
    */
    bool doGet(const QUrl& url);
    bool doPost(
        const QUrl& url,
        const nx_http::StringType& contentType,
        nx_http::StringType messageBody);

    bool doPut(
        const QUrl& url,
        const nx_http::StringType& contentType,
        nx_http::StringType messageBody);

    const Response* response() const;
    SystemError::ErrorCode lastSysErrorCode() const;
    bool isValid() const;
    bool eof() const;
    /**
        Blocks, if internal message body buffer is empty
    */
    BufferType fetchMessageBodyBuffer();
    void addAdditionalHeader(const StringType& key, const StringType& value);
    const QUrl& url() const;
    const QUrl& contentLocationUrl() const;
    StringType contentType() const;

    /** See \a AsyncHttpClient::setSubsequentReconnectTries */
    void setSubsequentReconnectTries(int reconnectTries);
    /** See \a AsyncHttpClient::setTotalReconnectTries */
    void setTotalReconnectTries(int reconnectTries);

    /** See \a AsyncHttpClient::setSendTimeoutMs */
    void setSendTimeoutMs(unsigned int sendTimeoutMs);
    /** See \a AsyncHttpClient::setResponseReadTimeoutMs */
    void setResponseReadTimeoutMs(unsigned int messageBodyReadTimeoutMs);
    /** See \a AsyncHttpClient::setMessageBodyReadTimeoutMs */
    void setMessageBodyReadTimeoutMs(unsigned int messageBodyReadTimeoutMs);
    void setUserAgent(const QString& userAgent);
    void setUserName(const QString& userAgent);
    void setUserPassword(const QString& userAgent);
    void setAuthType(AsyncHttpClient::AuthType value);
    void setProxyVia(const SocketAddress& proxyEndpoint);

    const std::unique_ptr<AbstractStreamSocket>& socket();
    /** Returns socket in blocking mode */
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

    void instanciateHttpClient();
    template<typename AsyncClientFunc>
        bool doRequest(AsyncClientFunc func);

    private slots:
    void onResponseReceived();
    void onSomeMessageBodyAvailable();
    void onDone();
    void onReconnected();
};

} // namespace nx_http
