// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <map>
#include <memory>

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/abstract_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/http_stream_reader.h>

namespace nx::network::http {

class AsyncHttpClientPtr;

/**
 * HTTP client. All operations are done asynchronously.
 *
 * To get new instance use AsyncHttpClient::create
 * This class methods are not thread-safe
 * All signals are emitted from io::AIOService threads
 * State is changed just before emitting signal
 * WARNING: It is strongly recommended to listen for AsyncHttpClient::someMessageBodyAvailable()
 * signal and read current message body buffer with AsyncHttpClient::fetchMessageBodyBuffer() call
 * every time to avoid internal message body buffer to consume too much memory.
 * WARNING: It is strongly recommended to connect to signals using Qt::DirectConnection and
 * slot MUST NOT use blocking calls.
 */
class NX_NETWORK_API AsyncHttpClient:
    public QObject,
    public std::enable_shared_from_this<AsyncHttpClient>,
    public nx::network::aio::AbstractPollable
{
    Q_OBJECT

public:
    using Timeouts = AsyncClient::Timeouts;

    static const int UNLIMITED_RECONNECT_TRIES = -1;

    AsyncHttpClient(ssl::AdapterFunc adapterFunc);
    AsyncHttpClient(std::unique_ptr<AbstractStreamSocket> socket, ssl::AdapterFunc adapterFunc);
    virtual ~AsyncHttpClient();

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;
    virtual void pleaseStopSync() override;

    virtual nx::network::aio::AbstractAioThread* getAioThread() const override;
    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;
    virtual void post(nx::utils::MoveOnlyFunc<void()> func) override;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> func) override;

    /**
     * Returns true if no response has been received due to transport error.
     */
    bool failed() const;
    SystemError::ErrorCode lastSysErrorCode() const;

    /**
     * Start GET request to url.
     * @return true, if socket is created and async connect is started. false otherwise.
     *   To get error description use SystemError::getLastOSErrorCode().
     */
    void doGet(const nx::utils::Url& url);
    /**
     * This overload is same as:
     * @code{.cpp}
     * QObject::connect(
     *     httpClient.get(), &nx::network::http::AsyncHttpClient::done,
     *     [completionHandler](AsyncHttpClientPtr httpClient)
     *     {
     *         completionHandler(httpClient);
     *     });
     * httpClient->doGet();
     * @endcode
     */
    void doGet(
        const nx::utils::Url& url,
        nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler);

    void doHead(const nx::utils::Url& url);
    void doHead(
        const nx::utils::Url& url,
        nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler);

    /**
     * Start POST request to url.
     *
     * TODO: #akolesnikov includeContentLength parameter is a workaround. Replace it with
     *     AbstractMsgBodySource.
     *
     * TODO: Support infinite POST message body.
     *
     * @return True, if the socket is created and async connection is started, false otherwise.
     */
    void doPost(
        const nx::utils::Url& url,
        const std::string& contentType,
        nx::Buffer messageBody,
        bool includeContentLength = true);
    void doPost(
        const nx::utils::Url& url,
        const std::string& contentType,
        nx::Buffer messageBody,
        bool includeContentLength,
        nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler);

    void doPut(
        const nx::utils::Url& url,
        const std::string& contentType,
        nx::Buffer messageBody);
    void doPut(
        const nx::utils::Url& url,
        const std::string& contentType,
        nx::Buffer messageBody,
        nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler);

    void doPatch(
        const nx::utils::Url& url,
        const std::string& contentType,
        nx::Buffer messageBody);
    void doPatch(
        const nx::utils::Url& url,
        const std::string& contentType,
        nx::Buffer messageBody,
        nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler);

    void doDelete(const nx::utils::Url& url);
    void doDelete(
        const nx::utils::Url& url,
        nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler);

    void doUpgrade(
        const nx::utils::Url& url,
        const std::string& protocolToUpgradeTo);
    void doUpgrade(
        const nx::utils::Url& url,
        const std::string& protocolToUpgradeTo,
        nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler);

    const nx::network::http::Request& request() const;

    /**
     * Response is valid only after signal responseReceived() has been emitted.
     * @return Can be NULL if no response has been received yet
     */
    const Response* response() const;
    std::string contentType() const;

    /**
     * Checks state as well as response return HTTP code (expect 2XX).
     */
    bool hasRequestSucceeded() const;

    /**
     * Returns current message body buffer, clearing it.
     * NOTE: This method is thread-safe and can be called in any thread.
     */
    nx::Buffer fetchMessageBodyBuffer();
    const nx::utils::Url& url() const;
    const nx::utils::Url& contentLocationUrl() const;
    /**
     * Number of bytes read (including http request line and headers)
     *  via single HTTP request.
     */
    quint64 bytesRead() const;

    /** By default, entity compression is on. */
    void setUseCompression(bool toggleUseEntityEncoding);
    void setSubsequentReconnectTries(int reconnectTries);
    void setTotalReconnectTries(int reconnectTries);
    void setUserAgent(const std::string& userAgent);

    void setCredentials(const Credentials& credentials);
    void setProxyCredentials(const Credentials& credentials);

    const Credentials& credentials() const;

    void setProxyVia(const SocketAddress& proxyEndpoint, bool isSecure, ssl::AdapterFunc adapterFunc);

    void setMaxNumberOfRedirects(int maxNumberOfRedirects);
    /**
     * If set to true client will not try to add Authorization header to the first request.
     * false by default.
     */
    void setDisablePrecalculatedAuthorization(bool val);

    /** Set socket connect/send timeout. */
    void setSendTimeoutMs(unsigned int sendTimeoutMs);
    /**
     * @param responseReadTimeoutMs 0 means infinity.
     * By default, 3000 ms.
     * If timeout has been met, connection is closed, state set to failed and AsyncHttpClient::done
     * emitted.
     */
    void setResponseReadTimeoutMs(unsigned int responseReadTimeoutMs);
    /**
     * @param messageBodyReadTimeoutMs 0 means infinity.
     * By default there is no timeout.
     * If timeout has been met, connection is closed, state set to failed and AsyncHttpClient::done
     * emitted.
     */
    void setMessageBodyReadTimeoutMs(unsigned int messageBodyReadTimeoutMs);

    const std::unique_ptr<AbstractStreamSocket>& socket();
    /**
     * Returns socket in non-blocking mode.
     * NOTE: Can be called within object's aio thread only.
     */
    std::unique_ptr<AbstractStreamSocket> takeSocket();

    void addAdditionalHeader(const std::string& key, const std::string& value);
    void addRequestHeaders(const HttpHeaders& headers);
    void removeAdditionalHeader(const std::string& key);
    void setAdditionalHeaders(HttpHeaders additionalHeaders);
    void setAuthType(AuthType value);

    void setAdapterFunc(ssl::AdapterFunc adapterFunc);

    /**
     * Use this method to instantiate AsyncHttpClient class.
     * @return smart pointer to newly created instance.
     */
    static AsyncHttpClientPtr create(ssl::AdapterFunc adapterFunc);
    static AsyncHttpClientPtr create(
        std::unique_ptr<AbstractStreamSocket> socket, ssl::AdapterFunc adapterFunc);

    static std::string endpointWithProtocol(const nx::utils::Url &url);

signals:
    /**
     * Invoked after request has been sent.
     * @param isRetryAfterUnauthorized set to true if request has been sent as after receiving 401 unauthorized response.
     */
    void requestHasBeenSent(nx::network::http::AsyncHttpClientPtr, bool isRetryAfterUnauthorizedResponse);
    /** Emitted when response headers has been read. */
    void responseReceived(nx::network::http::AsyncHttpClientPtr);
    /**
     * Message body buffer is not empty.
     * Received message body buffer is appended to internal buffer which
     *   can be read with AsyncHttpClient::fetchMessageBodyBuffer() call.
     * Responsibility for preventing internal message body buffer
     *   to grow beyond reasonable sizes lies on user of this class.
     * WARNING: It is strongly recommended to call AsyncHttpClient::fetchMessageBodyBuffer()
     *   every time on receiving this signal
    */
    void someMessageBodyAvailable(nx::network::http::AsyncHttpClientPtr);
    /**
     * Emitted when http request is done with any result
     *   (successfully executed request and received message body,
     *   received response with error code, connection terminated unexpectedly).
     * To get result code use method response().
     * NOTE: Some message body can still be stored in internal buffer.
     *   To read it, call AsyncHttpClient::fetchMessageBodyBuffer.
     */
    void done(nx::network::http::AsyncHttpClientPtr);
private:
    void initDelegate();
private:
    AsyncClient m_delegate;
    nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> m_onDoneHandler;

    virtual void stopWhileInAioThread();

    void onRequestHasBeenSent(bool isRetryAfterUnauthorizedResponse);
    void onResponseReceived();
    void onSomeMessageBodyAvailable();
    void onDone();

    AsyncHttpClient(const AsyncHttpClient&);
    AsyncHttpClient& operator=(const AsyncHttpClient&);
};

/**
 * Smart pointer for AsyncHttpClient.
 */
class NX_NETWORK_API AsyncHttpClientPtr
{
    typedef void (AsyncHttpClientPtr::*bool_type)() const;
    void this_type_does_not_support_comparisons() const {}

public:
    AsyncHttpClientPtr()
    {
    }

    AsyncHttpClientPtr(std::shared_ptr<AsyncHttpClient> obj):
        m_obj(std::move(obj))
    {
    }

    AsyncHttpClientPtr(const AsyncHttpClientPtr& right):
        m_obj(right.m_obj)
    {
    }

    AsyncHttpClientPtr(AsyncHttpClientPtr&& right) noexcept
    {
        std::swap(m_obj, right.m_obj);
    }

    AsyncHttpClientPtr& operator=(const AsyncHttpClientPtr& right)
    {
        if (this == &right)
            return *this;
        reset();
        m_obj = right.m_obj;
        return *this;
    }

    // This move assignment operator may throw in `reset()`, that breaks C++ Core Guidelines F.6 and
    // is very unusual. We mark it with explicit `noexcept(false)` and suppress the warning.
    AsyncHttpClientPtr& operator=(AsyncHttpClientPtr&& right)
        #if defined(_MSC_VER)
            #pragma warning(suppress: 26439)
        #endif
        noexcept(false)
    {
        if (this == &right)
            return *this;
        reset();
        std::swap(m_obj, right.m_obj);
        return *this;
    }

    ~AsyncHttpClientPtr()
    {
        reset();
    }

    AsyncHttpClient* operator->() const
    {
        return m_obj.get();
    }

    AsyncHttpClient& operator*() const
    {
        return *m_obj;
    }

    void reset()
    {
        // MUST call terminate BEFORE shared pointer destruction to allow for async handlers to
        // complete.
        if (m_obj.use_count() == 1)
        {
            // pleaseStopSync should have already been called explicitly.
            m_obj->pleaseStopSync();
        }
        m_obj.reset();
    }

    long use_count() const
    {
        return m_obj.use_count();
    }

    void swap(AsyncHttpClientPtr& right)
    {
        std::swap(m_obj, right.m_obj);
    }

    const AsyncHttpClient* get() const
    {
        return m_obj.get();
    }

    AsyncHttpClient* get()
    {
        return m_obj.get();
    }

    operator bool_type() const
    {
        return m_obj ? &AsyncHttpClientPtr::this_type_does_not_support_comparisons : 0;
    }

    bool operator<(const AsyncHttpClientPtr& right) const { return m_obj < right.m_obj; }
    bool operator<=(const AsyncHttpClientPtr& right) const { return m_obj <= right.m_obj; }
    bool operator>(const AsyncHttpClientPtr& right) const { return m_obj > right.m_obj; }
    bool operator>=(const AsyncHttpClientPtr& right) const { return m_obj >= right.m_obj; }
    bool operator==(const AsyncHttpClientPtr& right) const { return m_obj == right.m_obj; }
    bool operator!=(const AsyncHttpClientPtr& right) const { return m_obj != right.m_obj; }

private:
    std::shared_ptr<AsyncHttpClient> m_obj;
};

using DownloadCompletionHandler = std::function<void(
    SystemError::ErrorCode, int /*statusCode*/, nx::Buffer /*message body*/,
    nx::network::http::HttpHeaders /*response headers*/)>;

/**
 * Helper function that uses nx::network::http::AsyncHttpClient for file download.
 * @param completionHandler <OS error code, status code, message body, http response headers>.
 * "Status code", "message body" and "http response headers" are valid only if "OS error code"
 * is SystemError::noError.
 * @return true if started async download, false otherwise.
 * NOTE: It is strongly recommended to use this for downloading only small files (e.g., camera params).
*  params).
 * For real files better to use nx::network::http::AsyncHttpClient directly.
 */
void NX_NETWORK_API downloadFileAsync(
    ssl::AdapterFunc adapterFunc,
    const nx::utils::Url& url,
    DownloadCompletionHandler completionHandler,
    const http::HttpHeaders& extraHeaders = http::HttpHeaders(),
    AuthType authType = AuthType::authBasicAndDigest,
    AsyncHttpClient::Timeouts timeouts = {});

using DownloadCompletionHandlerEx = std::function<void(SystemError::ErrorCode, int /*statusCode*/,
    std::string /*content type*/, nx::Buffer /*message body*/,
    nx::network::http::HttpHeaders /*response headers*/)>;

/**
 * Same as downloadFileAsync but provide contentType at callback.
 */
void NX_NETWORK_API downloadFileAsyncEx(
    ssl::AdapterFunc adapterFunc,
    const nx::utils::Url& url,
    DownloadCompletionHandlerEx completionHandlerEx,
    const http::HttpHeaders& extraHeaders = http::HttpHeaders(),
    AuthType authType = AuthType::authBasicAndDigest,
    AsyncHttpClient::Timeouts timeouts = {},
    const std::string_view& method = nx::network::http::Method::get);

void downloadFileAsyncEx(
    const nx::utils::Url& url,
    std::function<void(SystemError::ErrorCode, int, std::string, nx::Buffer)> completionHandler,
    nx::network::http::AsyncHttpClientPtr httpClientCaptured);

using UploadCompletionHandler = std::function<void(SystemError::ErrorCode, int httpStatus)>;

/**
 * Uploads specified data using POST.
 */
void NX_NETWORK_API uploadDataAsync(
    ssl::AdapterFunc adapterFunc,
    const nx::utils::Url &url,
    const nx::Buffer& data,
    const std::string& contentType,
    const http::HttpHeaders& extraHeaders,
    const UploadCompletionHandler& callback,
    const AuthType authType = AuthType::authBasicAndDigest,
    const std::string& user = std::string(),
    const std::string& password = std::string(),
    const std::string_view& method = std::string_view(),
    std::optional<AsyncHttpClient::Timeouts> timeouts = std::nullopt);

SystemError::ErrorCode NX_NETWORK_API uploadDataSync(
    ssl::AdapterFunc adapterFunc,
    const nx::utils::Url& url,
    const nx::Buffer& data,
    const std::string& contentType,
    const std::string& user,
    const std::string& password,
    const AuthType authType = AuthType::authBasicAndDigest,
    http::StatusCode::Value* httpCode = nullptr);

} // namespace nx::network::http
