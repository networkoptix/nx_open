// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

#include <nx/network/async_stoppable.h>
#include <nx/utils/buffer.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/url.h>

#include "http_async_client.h"

namespace nx::network::http {

/**
 * Synchronous http client.
 * This is a synchronous wrapper on top of AsyncHttpClient.
 * NOTE: This class is not thread-safe.
 * WARNING: Message body is read asynchronously to some internal buffer.
 */
class NX_NETWORK_API HttpClient
{
public:
    static constexpr std::size_t kDefaultMaxInternalBufferSize = 200 * 1024 * 1024;

    HttpClient(ssl::AdapterFunc adapterFunc);
    HttpClient(
        std::unique_ptr<nx::network::AbstractStreamSocket> socket, ssl::AdapterFunc adapterFunc);
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
        const std::string& protocolToUpgradeTo);

    bool doUpgrade(
        const nx::utils::Url& url,
        Method method,
        const std::string& protocolToUpgradeTo);

    /**
     * @return true if response has been received.
     */
    bool doPost(
        const nx::utils::Url& url,
        std::unique_ptr<AbstractMsgBodySource> body);

    bool doPost(
        const nx::utils::Url& url,
        const std::string& contentType,
        nx::Buffer messageBody);

    bool doPost(const nx::utils::Url& url);

    /**
     * @return true if response has been received.
     */
    bool doPut(
        const nx::utils::Url& url,
        const std::string& contentType = {},
        nx::Buffer messageBody = {});

    /**
     * @return true if response has been received.
     */
    bool doPatch(
        const nx::utils::Url& url,
        const std::string& contentType = {},
        nx::Buffer messageBody = {});

    /**
     * @return true if response has been received.
     */
    bool doDelete(const nx::utils::Url& url);

    bool doConnect(const nx::utils::Url& proxyUrl, const std::string& targetHost);

    /**
     * @return true if response has been received.
     */
    bool doRequest(const Method& method, const nx::utils::Url& url);

    const Request& request() const;
    const Response* response() const;
    SystemError::ErrorCode lastSysErrorCode() const;
    bool isValid() const;
    bool eof() const;
    bool hasRequestSucceeded() const;

    /**
     * Retrieve the currently accumulated message body, removing it from the internal buffer.
     * Blocks while the internal message body buffer is empty. To read the whole message body,
     * should be called in a loop while not eof().
     */
    nx::Buffer fetchMessageBodyBuffer();

    /** Blocks until entire message body is available or timeout expired if it is specified. */
    std::optional<nx::Buffer> fetchEntireMessageBody(
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    void addAdditionalHeader(const std::string& key, const std::string& value);
    void removeAdditionalHeader(const std::string& key);
    const nx::utils::Url& url() const;
    const nx::utils::Url& contentLocationUrl() const;
    std::string contentType() const;

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

    /** Set all timeouts at once */
    void setTimeouts(const AsyncClient::Timeouts& timeouts);
    void setAllTimeouts(std::chrono::milliseconds timeout);

    AsyncClient::Timeouts timeouts() const;

    void setMaxNumberOfRedirects(int maxNumberOfRedirects);

    void setUserAgent(const std::string& userAgent);
    void setAuthType(AuthType value);

    void setCredentials(const Credentials& credentials);
    std::optional<Credentials> credentials() const;

    void setProxyCredentials(const Credentials& credentials);

    void setProxyVia(
        const SocketAddress& proxyEndpoint, bool isSecure, ssl::AdapterFunc adapterFunc);

    void setDisablePrecalculatedAuthorization(bool value);

    const std::unique_ptr<AbstractStreamSocket>& socket();

    /** @return Socket in blocking mode. */
    std::unique_ptr<AbstractStreamSocket> takeSocket();

    /**
     * @param customResponseReadTimeout If not specified, then default timeout is used.
     */
    static bool fetchResource(
        const nx::utils::Url& url,
        nx::Buffer* msgBody,
        std::string* contentType,
        std::optional<std::chrono::milliseconds> customResponseReadTimeout,
        ssl::AdapterFunc adapterFunc);

    int totalRequestsSentViaCurrentConnection() const;
    int totalRequestsSent() const;

    ssl::AdapterFunc setAdapterFunc(ssl::AdapterFunc adapterFunc);

    nx::network::http::StatusCode::Value statusCode() const;

private:
    std::unique_ptr<nx::network::http::AsyncClient> m_asyncHttpClient;
    nx::WaitCondition m_cond;
    mutable nx::Mutex m_mutex;
    bool m_done = false;
    bool m_error = false;
    bool m_terminated = false;
    nx::Buffer m_msgBodyBuffer;
    std::vector<std::pair<std::string, std::string>> m_additionalHeaders;
    std::optional<int> m_subsequentReconnectTries;
    std::optional<int> m_reconnectTries;
    AsyncClient::Timeouts m_timeouts;
    std::optional<int> m_maxNumberOfRedirects;
    std::optional<std::string> m_userAgent;
    std::size_t m_maxInternalBufferSize = kDefaultMaxInternalBufferSize;
    std::optional<SocketAddress> m_proxyEndpoint;
    bool m_isProxySecure = false;
    std::optional<AuthType> m_authType;
    std::optional<Credentials> m_credentials;
    std::optional<Credentials> m_proxyCredentials;
    ssl::AdapterFunc m_proxyAdapterFunc = ssl::kDefaultCertificateCheck;
    std::optional<Response> m_lastResponse;

    bool m_precalculatedAuthorizationDisabled = false;
    std::unique_ptr<nx::network::AbstractStreamSocket> m_socket;
    ssl::AdapterFunc m_adapterFunc;

    void instantiateAsyncClient();
    void configureAsyncClient();

    template<typename AsyncClientFunc>
        bool doRequest(AsyncClientFunc func);

    void onResponseReceived();
    void onSomeMessageBodyAvailable();
    void onDone();
};

} // namespace nx::network::http
