#pragma once

#include <chrono>
#include <map>
#include <memory>

#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/object_destruction_flag.h>

#include "abstract_msg_body_source.h"
#include "auth_cache.h"
#include "http_stream_reader.h"

namespace nx_http {

enum class AuthType
{
    authBasicAndDigest,
    authDigest,
    authDigestWithPasswordHash,
    authBasic,
};

/**
 * HTTP client. All operations are done asynchronously.
 *
 * All events (setOn...) are delivered within object's aio thread.
 * State is changed just before delivering event.
 * This class methods are not thread-safe.
 * NOTE: This class is a replacement for nx_http::AsyncHttpClient.
 *   As soon as it becomes ready, nx_http::AsyncHttpClient will be declared as deprecated.
 * WARNING: It is strongly recommended to listen for someMessageBodyAvailable() event and
 *   read current message body buffer with AsyncClient::fetchMessageBodyBuffer() call every time
 *   to avoid internal message body buffer to consume too much memory.
 */
class NX_NETWORK_API AsyncClient:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

public:
    enum class State
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

    AsyncClient();
    virtual ~AsyncClient();

    AsyncClient(const AsyncClient&) = delete;
    AsyncClient& operator=(const AsyncClient&) = delete;

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    State state() const;

    /**
     * @return true if no response has been recevied due to transport error.
     */
    bool failed() const;
    SystemError::ErrorCode lastSysErrorCode() const;

    /**
     * Invoked after request has been sent.
     * @param isRetryAfterUnauthorized set to true if request has been sent as after receiving 401 unauthorized response.
     */
    void setOnRequestHasBeenSent(
        nx::utils::MoveOnlyFunc<void(bool /*isRetryAfterUnauthorizedResponse*/)> handler);
    /** Emitted when response headers has been read. */
    void setOnResponseReceived(nx::utils::MoveOnlyFunc<void()> handler);
    /**
     * Message body buffer is not empty.
     * Received message body buffer is appended to internal buffer which
     *   can be read with AsyncClient::fetchMessageBodyBuffer() call.
     * Responsibility for preventing internal message body buffer
     *   to grow beyond reasonable sizes lies on user of this class.
     * WARNING: It is strongly recommended to call AsyncClient::fetchMessageBodyBuffer()
     *   every time on receiving this signal
    */
    void setOnSomeMessageBodyAvailable(nx::utils::MoveOnlyFunc<void()> handler);
    /**
     * Emitted when http request is done with any result
     *   (successfully executed request and received message body,
     *   received response with error code, connection terminated unexpectedly).
     * To get result code use method response().
     * @note Some message body can still be stored in internal buffer.
     *   To read it, call AsyncClient::fetchMessageBodyBuffer.
     */
    void setOnDone(nx::utils::MoveOnlyFunc<void()> handler);

    void setRequestBody(std::unique_ptr<AbstractMsgBodySource> body);

    /**
     * Start GET request to url.
     * @return true, if socket is created and async connect is started. false otherwise.
     *   To get error description use SystemError::getLastOSErrorCode().
     */
    void doGet(const QUrl& url);
    /**
     * This overload is same as:
     * @code{.cpp}
     * httpClient->setOnDone(func);
     * httpClient->doGet(url);
     * @endcode
     */
    void doGet(
        const QUrl& url,
        nx::utils::MoveOnlyFunc<void()> completionHandler);

    /**
     * Start POST request to url.
     * @return true, if socket is created and async connect is started. false otherwise
     */
    void doPost(const QUrl& url);
    void doPost(
        const QUrl& url,
        nx::utils::MoveOnlyFunc<void()> completionHandler);

    void doPut(const QUrl& url);
    void doPut(
        const QUrl& url,
        nx::utils::MoveOnlyFunc<void()> completionHandler);

    void doDelete(const QUrl& url);
    void doDelete(
        const QUrl& url,
        nx::utils::MoveOnlyFunc<void()> completionHandler);

    void doUpgrade(
        const QUrl& url,
        const StringType& protocolToUpgradeTo,
        nx::utils::MoveOnlyFunc<void()> completionHandler);
    void doUpgrade(
        const QUrl& url,
        nx_http::Method::ValueType method,
        const StringType& protocolToUpgradeTo,
        nx::utils::MoveOnlyFunc<void()> completionHandler);

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
    bool hasRequestSucceeded() const;

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

    /** If set to \a true client will not try to add Authorization header to the first request. false by default. */
    void setDisablePrecalculatedAuthorization(bool val);

    /** Set socket connect/send timeout. */
    void setSendTimeout(std::chrono::milliseconds sendTimeout);
    /**
     * @param responseReadTimeoutMs 0 means infinity.
     * By default, 3000 ms.
     * If timeout has been met, connection is closed, state set to failed and AsyncClient::done emitted.
     */
    void setResponseReadTimeout(std::chrono::milliseconds responseReadTimeout);
    /**
     * @param messageBodyReadTimeoutMs 0 means infinity.
     * By default there is no timeout.
     * If timeout has been met, connection is closed, state set to failed and AsyncClient::done emitted.
     */
    void setMessageBodyReadTimeout(std::chrono::milliseconds messageBodyReadTimeout);

    const std::unique_ptr<AbstractStreamSocket>& socket();
    /**
     * Returns socket in non-blocking mode.
     * @note Can be called within object's aio thread only.
     */
    std::unique_ptr<AbstractStreamSocket> takeSocket();

    void addAdditionalHeader(const StringType& key, const StringType& value);
    void addRequestHeaders(const HttpHeaders& headers);
    void removeAdditionalHeader(const StringType& key);
    void setAdditionalHeaders(HttpHeaders additionalHeaders);
    void setAuthType(AuthType value);
    AuthInfoCache::AuthorizationCacheItem authCacheItem() const;
    /**
     * Caller uses it to report that message body has ended (it may be tricky to detect message body end in some cases).
     * @note May be invoked within someMessageBodyAvailable handler only.
     * WARNING: It is a hack. Use it only if you strongly know what you are doing.
     */
    void forceEndOfMsgBody();

    void setExpectOnlyMessageBodyWithoutHeaders(bool expectOnlyBody);

    static QString endpointWithProtocol(const QUrl& url);

private:
    enum class Result
    {
        thisDestroyed,
        proceed,
    };

    nx::utils::MoveOnlyFunc<void(bool /*isRetryAfterUnauthorizedResponse*/)> m_onRequestHasBeenSent;
    nx::utils::MoveOnlyFunc<void()> m_onResponseReceived;
    nx::utils::MoveOnlyFunc<void()> m_onSomeMessageBodyAvailable;
    nx::utils::MoveOnlyFunc<void()> m_onDone;

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
    BufferType m_receivedBytesLeft;
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
    quint64 m_totalBytesReadPerRequest; //< total read bytes per request
    int m_totalRequestsSentViaCurrentConnection; //< total sent requests via single connection
    bool m_contentEncodingUsed;
    std::chrono::milliseconds m_sendTimeout;
    std::chrono::milliseconds m_responseReadTimeout;
    std::chrono::milliseconds m_msgBodyReadTimeout;
    AuthType m_authType;
    HttpHeaders m_additionalHeaders;
    int m_awaitedMessageNumber;
    QString m_remoteEndpointWithProtocol;
    AuthInfoCache::AuthorizationCacheItem m_authCacheItem;
    SystemError::ErrorCode m_lastSysErrorCode;
    int m_requestSequence;
    bool m_forcedEof;
    bool m_precalculatedAuthorizationDisabled;
    int m_numberOfRedirectsTried;
    nx::utils::ObjectDestructionFlag m_objectDestructionFlag;
    std::unique_ptr<AbstractMsgBodySource> m_requestBody;
    bool m_expectOnlyBody = false;

    virtual void stopWhileInAioThread() override;

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
    void processReceivedBytes(std::size_t bytesParsed);
    Result processResponseHeadersBytes(bool* const continueReceiving);
    bool isMalformed(const nx_http::Response& response) const;
    bool repeatRequestIfNeeded(const Response& response);
    bool sendRequestToNewLocation(const Response& response);
    Result processResponseMessageBodyBytes(std::size_t bytesRead, bool* const continueReceiving);
    void composeRequest(const nx_http::StringType& httpMethod);
    void addBodyToRequest();
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

    Result emitDone();
    Result emitRequestHasBeenSent(bool authorizationTried);
    Result emitResponseReceived();
    Result emitSomeMessageBodyAvailable();

    template<typename ... Args>
    Result invokeHandler(
        nx::utils::MoveOnlyFunc<void(Args...)>& handler,
        Args... args);

    static const char* toString(State state);
};

} // namespace nx_http
