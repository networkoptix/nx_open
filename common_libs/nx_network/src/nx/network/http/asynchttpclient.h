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

#include "auth_cache.h"
#include "http_stream_reader.h"

#include <nx/fusion/model_functions_fwd.h>

namespace nx_http {

struct AuthInfo
{
    QString username;
    QString password;
    QString proxyUsername;
    QString proxyPassword;
    SocketAddress proxyEndpoint;
};


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
    enum AuthType
    {
        authBasicAndDigest,
        authDigest,
        authDigestWithPasswordHash,
        authBasic,
    };

    enum State
    {
        sInit,
        sWaitingConnectToHost,
        sSendingRequest,
        sReceivingResponse,
        sResponseReceived,
        sReadingMessageBody,
        sFailed,
        sDone
    };

    /**
     * 0 means infinity for any timeout.
     */
    class NX_NETWORK_API Timeouts
    {
    public:
        constexpr static const std::chrono::seconds kDefaultSendTimeout =
            std::chrono::seconds(3);
        constexpr static const std::chrono::seconds kDefaultResponseReadTimeout =
            std::chrono::seconds(3);
        constexpr static const std::chrono::seconds kDefaultMessageBodyReadTimeout =
            std::chrono::seconds::zero();  //no timeout

        std::chrono::milliseconds sendTimeout;
        std::chrono::milliseconds responseReadTimeout;
        std::chrono::milliseconds messageBodyReadTimeout;

        Timeouts(
            std::chrono::milliseconds send = kDefaultSendTimeout,
            std::chrono::milliseconds recv = kDefaultResponseReadTimeout,
            std::chrono::milliseconds msgBody = kDefaultMessageBodyReadTimeout);

        bool operator==(const Timeouts& rhs) const;
    };

    static const int UNLIMITED_RECONNECT_TRIES = -1;

    virtual ~AsyncHttpClient();

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;
    virtual void pleaseStopSync(bool checkForLocks = true) override;

    virtual nx::network::aio::AbstractAioThread* getAioThread() const override;
    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;
    virtual void post(nx::utils::MoveOnlyFunc<void()> func) override;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> func) override;

    State state() const;

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
    void doGet(const QUrl& url);
    /**
     * This overload is same as:
     * @code{.cpp}
     * QObject::connect(
     *     httpClient.get(), &nx_http::AsyncHttpClient::done,
     *     [completionHandler](AsyncHttpClientPtr httpClient)
     *     {
     *         completionHandler(httpClient);
     *     });
     * httpClient->doGet();
     * @endcode
     */
    void doGet(
        const QUrl& url,
        nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler);

    /**
     * Start POST request to url.
     * @param includeContentLength TODO #ak this parameter is a hack. Replace it with AbstractMsgBodySource if future version
     * @return true, if socket is created and async connect is started. false otherwise
     * @todo Infinite POST message body support
     */
    void doPost(
        const QUrl& url,
        const nx_http::StringType& contentType,
        nx_http::StringType messageBody,
        bool includeContentLength = true);
    void doPost(
        const QUrl& url,
        const nx_http::StringType& contentType,
        nx_http::StringType messageBody,
        bool includeContentLength,
        nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler);

    void doPut(
        const QUrl& url,
        const nx_http::StringType& contentType,
        nx_http::StringType messageBody);
    void doPut(
        const QUrl& url,
        const nx_http::StringType& contentType,
        nx_http::StringType messageBody,
        nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler);

    void doDelete(const QUrl& url);
    void doDelete(
        const QUrl& url,
        nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler);

    void doOptions(const QUrl& url);
    void doOptions(
        const QUrl& url,
        nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler);

    void doUpgrade(
        const QUrl& url,
        const StringType& protocolToUpgradeTo);
    void doUpgrade(
        const QUrl& url,
        const StringType& protocolToUpgradeTo,
        nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler);

    const nx_http::Request& request() const;

    /**
     * Response is valid only after signal responseReceived() has been emitted.
     * @return Can be NULL if no response has been received yet
     */
    const Response* response() const;
    StringType contentType() const;

    /**
     * Checks state as well as response return HTTP code (expect 2XX).
     */
    bool hasRequestSuccesed() const;

    /**
     * Returns current message body buffer, clearing it.
     * @note This method is thread-safe and can be called in any thread.
     */
    BufferType fetchMessageBodyBuffer();
    const QUrl& url() const;
    const QUrl& contentLocationUrl() const;
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
    void setProxyUserName(const QString& userName);
    void setProxyUserPassword(const QString& userPassword);
    void setAuth(const AuthInfo& auth);
    void setProxyVia(const SocketAddress& proxyEndpoint);
    void setConnectionHeader(const StringType& value);

    /** If set to \a true client will not try to add Authorization header to the first request. false by default. */
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
     * @note Can be called within object's aio thread only.
     */
    std::unique_ptr<AbstractStreamSocket> takeSocket();

    void addAdditionalHeader(const StringType& key, const StringType& value);
    void addRequestHeaders(const HttpHeaders& headers);
    void removeAdditionalHeader(const StringType& key);
    template<class HttpHeadersRef>
    void setAdditionalHeaders(HttpHeadersRef&& additionalHeaders)
    {
        m_additionalHeaders = std::forward<HttpHeadersRef>(additionalHeaders);
    }
    void setAuthType(AuthType value);

    void setExpectOnlyMessageBodyWithoutHeaders(bool expectOnlyBody);

    AuthInfoCache::AuthorizationCacheItem authCacheItem() const;
    /**
     * Caller uses it to report that message body has ended (it may be tricky to detect message body end in some cases).
     * @note May be invoked within someMessageBodyAvailable handler only.
     * WARNING: It is a hack. Use it only if you strongly know what you are doing.
     */
    void forceEndOfMsgBody();

    /**
     * Use this method to intanciate AsyncHttpClient class.
     * @return smart pointer to newly created instance.
     */
    static AsyncHttpClientPtr create();

    static QString endpointWithProtocol(const QUrl& url);

signals:
    void tcpConnectionEstablished(nx_http::AsyncHttpClientPtr);
    /**
     * Invoked after request has been sent.
     * @param isRetryAfterUnauthorized set to true if request has been sent as after receiving 401 unauthorized response.
     */
    void requestHasBeenSent(nx_http::AsyncHttpClientPtr, bool isRetryAfterUnauthorizedResponse);
    /** Emitted when response headers has been read. */
    void responseReceived(nx_http::AsyncHttpClientPtr);
    /**
     * Message body buffer is not empty.
     * Received message body buffer is appended to internal buffer which
     *   can be read with AsyncHttpClient::fetchMessageBodyBuffer() call.
     * Responsibility for preventing internal message body buffer
     *   to grow beyond reasonable sizes lies on user of this class.
     * WARNING: It is strongly recommended to call AsyncHttpClient::fetchMessageBodyBuffer()
     *   every time on receiving this signal
    */
    void someMessageBodyAvailable(nx_http::AsyncHttpClientPtr);
    /**
     * Emitted when http request is done with any result
     *   (successfully executed request and received message body,
     *   received response with error code, connection terminated unexpectedly).
     * To get result code use method response().
     * @note Some message body can still be stored in internal buffer.
     *   To read it, call AsyncHttpClient::fetchMessageBodyBuffer.
     */
    void done(nx_http::AsyncHttpClientPtr);
    /** Connection to server has been restored after a sudden disconnect. */
    void reconnected(nx_http::AsyncHttpClientPtr);

private:
    State m_state;
    Request m_request;
    std::unique_ptr<AbstractStreamSocket> m_socket;
    bool m_connectionClosed;
    BufferType m_requestBuffer;
    size_t m_requestBytesSent;
    QUrl m_requestUrl;
    QUrl m_contentLocationUrl;
    HttpStreamReader m_httpStreamReader;
    BufferType m_responseBuffer;
    QString m_userAgent;
    QString m_userName;
    QString m_userPassword;
    QString m_proxyUserName;
    QString m_proxyUserPassword;
    boost::optional<SocketAddress> m_proxyEndpoint;
    bool m_authorizationTried;
    bool m_proxyAuthorizationTried;
    bool m_ha1RecalcTried;
    bool m_terminated;
    quint64 m_bytesRead; //< total read bytes per request
    int m_totalRequests; //< total sent requests via single connection
    bool m_contentEncodingUsed;
    unsigned int m_sendTimeoutMs;
    unsigned int m_responseReadTimeoutMs;
    unsigned int m_msgBodyReadTimeoutMs;
    AuthType m_authType;
    HttpHeaders m_additionalHeaders;
    int m_awaitedMessageNumber;
    QString m_remoteEndpointWithProtocol;
    AuthInfoCache::AuthorizationCacheItem m_authCacheItem;
    SystemError::ErrorCode m_lastSysErrorCode;
    int m_requestSequence;
    bool m_forcedEof;
    // TODO: #ak remove this member (replace with aio::BasicPollable inheritance).
    nx::network::aio::Timer m_aioThreadBinder;
    bool m_precalculatedAuthorizationDisabled;
    int m_numberOfRedirectsTried;

    bool m_expectOnlyBody;

    AsyncHttpClient();

    void asyncConnectDone(SystemError::ErrorCode errorCode);
    void asyncSendDone(SystemError::ErrorCode errorCode, size_t bytesWritten);
    void onSomeBytesReadAsync(SystemError::ErrorCode errorCode, size_t bytesRead);

    void resetDataBeforeNewRequest();
    void initiateHttpMessageDelivery();
    void initiateTcpConnection();
    /**
     * @return Bytes parsed or -1 in case of error.
     */
    size_t parseReceivedBytes(size_t bytesRead);
    void processReceivedBytes(
        std::shared_ptr<AsyncHttpClient> sharedThis,
        std::size_t bytesParsed);
    void processResponseHeadersBytes(
        std::shared_ptr<AsyncHttpClient> sharedThis,
        bool* const continueReceiving);
    bool repeatRequestIfNeeded(const Response& response);
    bool sendRequestToNewLocation(const Response& response);
    void processResponseMessageBodyBytes(
        std::shared_ptr<AsyncHttpClient> sharedThis,
        std::size_t bytesRead,
        bool* const continueReceiving);
    void composeRequest(const nx_http::StringType& httpMethod);
    void serializeRequest();
    /**
     * @return true, if connected.
     */
    bool reconnectIfAppropriate();
    /** Composes request with authorization header based on response. */
    bool resendRequestWithAuthorization(
        const nx_http::Response& response,
        bool isProxy = false);
    void doSomeCustomLogic(
        const nx_http::Response& response,
        Request* const request);
    void stopWhileInAioThread();

    template<typename ... Args>
    void doHttpOperation(
        nx::utils::MoveOnlyFunc<void(AsyncHttpClientPtr)> completionHandler,
        void(AsyncHttpClient::*func)(Args...),
        Args... args);

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


/**
 * Helper function that uses nx_http::AsyncHttpClient for file download.
 * @param completionHandler <OS error code, status code, message body>.
 * "Status code" and "message body" are valid only if "OS error code" is SystemError::noError
 * @return true if started async download, false otherwise.
 * @note It is strongly recommended to use this for downloading only small files (e.g., camera params).
 * For real files better to use nx_http::AsyncHttpClient directly.
 */
void NX_NETWORK_API downloadFileAsync(
    const QUrl& url,
    std::function<void(SystemError::ErrorCode, int /*statusCode*/, nx_http::BufferType)> completionHandler,
    const nx_http::HttpHeaders& extraHeaders = nx_http::HttpHeaders(),
    AsyncHttpClient::AuthType authType = AsyncHttpClient::authBasicAndDigest,
    AsyncHttpClient::Timeouts timeouts = AsyncHttpClient::Timeouts());

/**
 * Calls previous function and waits for completion.
 */
SystemError::ErrorCode NX_NETWORK_API downloadFileSync(
    const QUrl& url,
    int* const statusCode,
    nx_http::BufferType* const msgBody);

/**
 * Same as downloadFileAsync but provide contentType at callback.
 */
void NX_NETWORK_API downloadFileAsyncEx(
    const QUrl& url,
    std::function<void(SystemError::ErrorCode, int /*statusCode*/, nx_http::StringType /*contentType*/, nx_http::BufferType /*msgBody */)> completionHandler,
    const nx_http::HttpHeaders& extraHeaders = nx_http::HttpHeaders(),
    AsyncHttpClient::AuthType authType = AsyncHttpClient::authBasicAndDigest,
    AsyncHttpClient::Timeouts timeouts = AsyncHttpClient::Timeouts());

void downloadFileAsyncEx(
    const QUrl& url,
    std::function<void(SystemError::ErrorCode, int, nx_http::StringType, nx_http::BufferType)> completionHandler,
    nx_http::AsyncHttpClientPtr httpClientCaptured);

typedef std::function<void(SystemError::ErrorCode, int httpStatus)> UploadCompletionHandler;

/**
 * Uploads specified data using POST.
 */
void NX_NETWORK_API uploadDataAsync(const QUrl &url
    , const QByteArray &data
    , const QByteArray &contentType
    , const nx_http::HttpHeaders &extraHeaders
    , const UploadCompletionHandler &callback
    , const AsyncHttpClient::AuthType authType = AsyncHttpClient::authBasicAndDigest
    , const QString &user = QString()
    , const QString &password = QString());

SystemError::ErrorCode NX_NETWORK_API uploadDataSync(const QUrl &url
    , const QByteArray &data
    , const QByteArray &contentType
    , const QString &user
    , const QString &password
    , const AsyncHttpClient::AuthType authType = AsyncHttpClient::authBasicAndDigest
    , nx_http::StatusCode::Value *httpCode = nullptr);

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(nx_http::AsyncHttpClient::AuthType)

} // namespace nx_http

QN_FUSION_DECLARE_FUNCTIONS(nx_http::AsyncHttpClient::AuthType, (lexical), NX_NETWORK_API)
