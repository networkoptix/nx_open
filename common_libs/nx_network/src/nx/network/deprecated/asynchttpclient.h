#pragma once

#include <chrono>
#include <map>
#include <memory>

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>
#include <QSharedPointer>

#include "nx/network/abstract_socket.h"
#include "nx/network/aio/abstract_pollable.h"
#include "nx/network/aio/timer.h"
#include "nx/network/http/auth_cache.h"
#include "nx/network/http/http_async_client.h"
#include "nx/network/http/http_stream_reader.h"

namespace nx {
namespace network {
namespace http {

class AsyncHttpClientPtr;

/**
 * HTTP client. All operations are done asynchronously.
 *
 * To get new instance use AsyncHttpClient::create
 * This class methods are not thread-safe
 * All signals are emitted from io::AIOService threads
 * State is changed just before emitting signal
 * WARNING: It is strongly recommended to listen for AsyncHttpClient::someMessageBodyAvailable() signal and
 *  read current message body buffer with AsyncHttpClient::fetchMessageBodyBuffer() call every time
 *  to avoid internal message body buffer to consume too much memory.
 * WARNING: It is strongly recommended to connect to signals using Qt::DirectConnection and slot MUST NOT use blocking calls.
 */
class NX_NETWORK_API AsyncHttpClient:
    public QObject,
    public std::enable_shared_from_this<AsyncHttpClient>,
    public nx::network::aio::AbstractPollable
{
    Q_OBJECT

public:
    using State = AsyncClient::State;
    using Timeouts = AsyncClient::Timeouts;

    static const int UNLIMITED_RECONNECT_TRIES = -1;

    AsyncHttpClient();
    virtual ~AsyncHttpClient();

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;
    virtual void pleaseStopSync(bool checkForLocks = true) override;

    virtual nx::network::aio::AbstractAioThread* getAioThread() const override;
    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;
    virtual void post(nx::utils::MoveOnlyFunc<void()> func) override;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> func) override;

    AsyncClient::State state() const;

    /**
     * Returns true if no response has been recevied due to transport error.
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

    /**
     * Start POST request to url.
     * @param includeContentLength TODO #ak this parameter is a hack. Replace it with AbstractMsgBodySource if future version
     * @return true, if socket is created and async connect is started. false otherwise
     * @todo Infinite POST message body support
     */
    void doPost(
        const nx::utils::Url& url,
        const nx::network::http::StringType& contentType,
        nx::network::http::StringType messageBody,
        bool includeContentLength = true);
    void doPost(
        const nx::utils::Url& url,
        const nx::network::http::StringType& contentType,
        nx::network::http::StringType messageBody,
        bool includeContentLength,
        nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler);

    void doPut(
        const nx::utils::Url& url,
        const nx::network::http::StringType& contentType,
        nx::network::http::StringType messageBody);
    void doPut(
        const nx::utils::Url& url,
        const nx::network::http::StringType& contentType,
        nx::network::http::StringType messageBody,
        nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler);

    void doDelete(const nx::utils::Url& url);
    void doDelete(
        const nx::utils::Url& url,
        nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler);

    void doUpgrade(
        const nx::utils::Url& url,
        const StringType& protocolToUpgradeTo);
    void doUpgrade(
        const nx::utils::Url& url,
        const StringType& protocolToUpgradeTo,
        nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler);

    const nx::network::http::Request& request() const;

    /**
     * Response is valid only after signal responseReceived() has been emitted.
     * @return Can be NULL if no response has been received yet
     */
    const Response* response() const;
    StringType contentType() const;

    /**
     * Checks state as well as response return HTTP code (expect 2XX).
     */
    bool hasRequestSucceeded() const;

    /**
     * Returns current message body buffer, clearing it.
     * NOTE: This method is thread-safe and can be called in any thread.
     */
    BufferType fetchMessageBodyBuffer();
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
    void setUserAgent(const QString& userAgent);

    void setUserName(const QString& userName);
    void setUserPassword(const QString& userPassword);
    void setUserAuthToken(const AuthToken& userToken);
    void setUserCredentials(const Credentials& userCredentials);
    void setProxyUserName(const QString& userName);
    void setProxyUserPassword(const QString& userPassword);
    void setProxyUserAuthToken(const AuthToken& proxyUserToken);
    void setProxyUserCredentials(const Credentials& userCredentials);
    void setAuth(const AuthInfo& auth);

    void setProxyVia(const SocketAddress& proxyEndpoint);

    /** If set to true client will not try to add Authorization header to the first request. false by default. */
    void setDisablePrecalculatedAuthorization(bool val);

    /** Set socket connect/send timeout. */
    void setSendTimeoutMs(unsigned int sendTimeoutMs);
    /**
     * @param responseReadTimeoutMs 0 means infinity.
     * By default, 3000 ms.
     * If timeout has been met, connection is closed, state set to failed and AsyncHttpClient::done emitted.
     */
    void setResponseReadTimeoutMs(unsigned int responseReadTimeoutMs);
    /**
     * @param messageBodyReadTimeoutMs 0 means infinity.
     * By default there is no timeout.
     * If timeout has been met, connection is closed, state set to failed and AsyncHttpClient::done emitted.
     */
    void setMessageBodyReadTimeoutMs(unsigned int messageBodyReadTimeoutMs);

    const std::unique_ptr<AbstractStreamSocket>& socket();
    /**
     * Returns socket in non-blocking mode.
     * NOTE: Can be called within object's aio thread only.
     */
    std::unique_ptr<AbstractStreamSocket> takeSocket();

    void addAdditionalHeader(const StringType& key, const StringType& value);
    void addRequestHeaders(const HttpHeaders& headers);
    void removeAdditionalHeader(const StringType& key);
    void setAdditionalHeaders(HttpHeaders additionalHeaders);
    void setAuthType(AuthType value);

    void setExpectOnlyMessageBodyWithoutHeaders(bool expectOnlyBody);

    AuthInfoCache::AuthorizationCacheItem authCacheItem() const;
    /**
     * Caller uses it to report that message body has ended (it may be tricky to detect message body end in some cases).
     * NOTE: May be invoked within someMessageBodyAvailable handler only.
     * WARNING: It is a hack. Use it only if you strongly know what you are doing.
     */
    void forceEndOfMsgBody();

    /**
     * Use this method to intanciate AsyncHttpClient class.
     * @return smart pointer to newly created instance.
     */
    static AsyncHttpClientPtr create();

    static QString endpointWithProtocol(const nx::utils::Url &url);

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
    AsyncClient m_delegate;
    nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> m_onDoneHandler;

    virtual void stopWhileInAioThread();

    void onRequestHasBeenSent(bool isRetryAfterUnauthorizedResponse);
    void onResponseReceived();
    void onSomeMessageBodyAvailable();
    void onDone();

    AsyncHttpClient(const AsyncHttpClient&);
    AsyncHttpClient& operator=(const AsyncHttpClient&);

    static const char* toString(State state);
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

    AsyncHttpClientPtr(AsyncHttpClientPtr&& right)
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

    AsyncHttpClientPtr& operator=(AsyncHttpClientPtr&& right)
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
        // MUST call terminate BEFORE shared pointer destruction to allow for async handlers to complete.
        if (m_obj.use_count() == 1)
            m_obj->pleaseStopSync(false);   //< pleaseStopSync should have already been called explicitly.
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

inline QDebug& operator<<(QDebug& stream, const AsyncHttpClientPtr& value)
{
    return stream << toString(value.get());
}

/**
 * Helper function that uses nx::network::http::AsyncHttpClient for file download.
 * @param completionHandler <OS error code, status code, message body>.
 * "Status code" and "message body" are valid only if "OS error code" is SystemError::noError
 * @return true if started async download, false otherwise.
 * NOTE: It is strongly recommended to use this for downloading only small files (e.g., camera params).
 * For real files better to use nx::network::http::AsyncHttpClient directly.
 */
void NX_NETWORK_API downloadFileAsync(
    const nx::utils::Url& url,
    std::function<void(SystemError::ErrorCode, int /*statusCode*/, nx::network::http::BufferType)> completionHandler,
    const nx::network::http::HttpHeaders& extraHeaders = nx::network::http::HttpHeaders(),
    AuthType authType = AuthType::authBasicAndDigest,
    AsyncHttpClient::Timeouts timeouts = AsyncHttpClient::Timeouts());

/**
 * Calls previous function and waits for completion.
 */
SystemError::ErrorCode NX_NETWORK_API downloadFileSync(
    const nx::utils::Url& url,
    int* const statusCode,
    nx::network::http::BufferType* const msgBody);

/**
 * Same as downloadFileAsync but provide contentType at callback.
 */
void NX_NETWORK_API downloadFileAsyncEx(
    const nx::utils::Url& url,
    std::function<void(SystemError::ErrorCode, int /*statusCode*/, nx::network::http::StringType /*contentType*/, nx::network::http::BufferType /*msgBody */)> completionHandler,
    const nx::network::http::HttpHeaders& extraHeaders = nx::network::http::HttpHeaders(),
    AuthType authType = AuthType::authBasicAndDigest,
    AsyncHttpClient::Timeouts timeouts = AsyncHttpClient::Timeouts());

void downloadFileAsyncEx(
    const nx::utils::Url& url,
    std::function<void(SystemError::ErrorCode, int, nx::network::http::StringType, nx::network::http::BufferType)> completionHandler,
    nx::network::http::AsyncHttpClientPtr httpClientCaptured);

typedef std::function<void(SystemError::ErrorCode, int httpStatus)> UploadCompletionHandler;

/**
 * Uploads specified data using POST.
 */
void NX_NETWORK_API uploadDataAsync(
    const nx::utils::Url &url,
    const QByteArray& data,
    const QByteArray& contentType,
    const nx::network::http::HttpHeaders& extraHeaders,
    const UploadCompletionHandler& callback,
    const AuthType authType = AuthType::authBasicAndDigest,
    const QString& user = QString(),
    const QString& password = QString());

SystemError::ErrorCode NX_NETWORK_API uploadDataSync(
    const nx::utils::Url& url,
    const QByteArray& data,
    const QByteArray& contentType,
    const QString& user,
    const QString& password,
    const AuthType authType = AuthType::authBasicAndDigest,
    nx::network::http::StatusCode::Value* httpCode = nullptr);

} // namespace nx
} // namespace network
} // namespace http
