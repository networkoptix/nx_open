#pragma once

#include <chrono>

#include <nx/utils/std/optional.h>

#include <nx/network/async_stoppable.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>

#include "http_async_client.h"

namespace nx {
namespace network {
namespace http {

/**
 * Synchronous http client.
 * This is a synchronous wrapper on top of AsyncHttpClient.
 * NOTE: This class is not thread-safe.
 * WARNING: Message body is read asynchronously to some internal buffer.
 */
class NX_NETWORK_API HttpClient
{
public:
    HttpClient();
    HttpClient(std::unique_ptr<nx::network::AbstractStreamSocket> socket);
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
    std::optional<BufferType> fetchEntireMessageBody();

    void addAdditionalHeader(const StringType& key, const StringType& value);
    void removeAdditionalHeader(const StringType& key);
    const nx::utils::Url& url() const;
    const nx::utils::Url& contentLocationUrl() const;
    StringType contentType() const;

    /** See AsyncHttpClient::setSubsequentReconnectTries */
    void setSubsequentReconnectTries(int reconnectTries);

    /** See AsyncHttpClient::setTotalReconnectTries */
    void setTotalReconnectTries(int reconnectTries);

    /** See http::AsyncClient::setSendTimeout */
    void setSendTimeout(std::chrono::milliseconds sendTimeout);

    /** See http::AsyncClient::setResponseReadTimeout */
    void setResponseReadTimeout(std::chrono::milliseconds messageBodyReadTimeout);

    /** See http::AsyncClient::setMessageBodyReadTimeout */
    void setMessageBodyReadTimeout(std::chrono::milliseconds messageBodyReadTimeout);

    void setMaxNumberOfRedirects(int maxNumberOfRedirects);

    void setUserAgent(const QString& userAgent);
    void setUserName(const QString& userAgent);
    void setUserPassword(const QString& userAgent);
    void setAuthType(AuthType value);
    void setProxyVia(const SocketAddress& proxyEndpoint, bool isSecure);

    void setDisablePrecalculatedAuthorization(bool value);
    void setExpectOnlyMessageBodyWithoutHeaders(bool expectOnlyBody);

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
        std::optional<std::chrono::milliseconds> customResponseReadTimeout);

    int totalRequestsSentViaCurrentConnection() const;
    int totalRequestsSent() const;
private:
    std::unique_ptr<nx::network::http::AsyncClient> m_asyncHttpClient;
    QnWaitCondition m_cond;
    mutable QnMutex m_mutex;
    bool m_done;
    bool m_error;
    bool m_terminated;
    nx::network::http::BufferType m_msgBodyBuffer;
    std::vector<std::pair<StringType, StringType>> m_additionalHeaders;
    std::optional<int> m_subsequentReconnectTries;
    std::optional<int> m_reconnectTries;
    std::optional<std::chrono::milliseconds> m_sendTimeout;
    std::optional<std::chrono::milliseconds> m_responseReadTimeout;
    std::optional<std::chrono::milliseconds> m_messageBodyReadTimeout;
    std::optional<int> m_maxNumberOfRedirects;
    std::optional<QString> m_userAgent;
    std::optional<QString> m_userName;
    std::optional<QString> m_userPassword;
    std::size_t m_maxInternalBufferSize;
    std::optional<SocketAddress> m_proxyEndpoint;
    bool m_isProxySecure = false;
    std::optional<AuthType> m_authType;

    bool m_precalculatedAuthorizationDisabled = false;
    bool m_expectOnlyBody = false;
    std::unique_ptr<nx::network::AbstractStreamSocket> m_socket;

    void instantiateHttpClient();
    template<typename AsyncClientFunc>
        bool doRequest(AsyncClientFunc func);

    void onResponseReceived();
    void onSomeMessageBodyAvailable();
    void onDone();
};

} // namespace nx
} // namespace network
} // namespace http
