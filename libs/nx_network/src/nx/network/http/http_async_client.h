// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <string>

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/interruption_flag.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/url.h>

#include "abstract_msg_body_source.h"
#include "auth_tools.h"
#include "http_stream_reader.h"
#include "server/abstract_authentication_manager.h"
#include "server/http_server_connection.h"

namespace nx::network::http {

/**
 * Accumulates HTTP client options. Should be used to provide HTTP options in classes that use
 * nx::network::http::AsyncClient internaly without providing direct access to it.
 */
class NX_NETWORK_API ClientOptions
{
public:
    /**
     * 0 means infinity for any timeout.
     */
    class NX_NETWORK_API Timeouts
    {
    public:
        std::chrono::milliseconds sendTimeout = defaults().sendTimeout;
        std::chrono::milliseconds responseReadTimeout = defaults().responseReadTimeout;
        std::chrono::milliseconds messageBodyReadTimeout = defaults().messageBodyReadTimeout;

        constexpr bool operator==(const Timeouts& rhs) const
        {
            return sendTimeout == rhs.sendTimeout
                && responseReadTimeout == rhs.responseReadTimeout
                && messageBodyReadTimeout == rhs.messageBodyReadTimeout;
        }

        static const Timeouts& defaults();

        /** Is not thread-safe, should be called before any network use. */
        static void setDefaults(Timeouts value);
    };

    static constexpr Timeouts kInfiniteTimeouts = {
        std::chrono::milliseconds::zero(),
        std::chrono::milliseconds::zero(),
        std::chrono::milliseconds::zero() };

    /** By default, entity compression is on. */
    void setUseCompression(bool toggleUseEntityEncoding);
    bool useCompression() const;

    void setSubsequentReconnectTries(int reconnectTries);
    int subsequentReconnectTries() const;

    void setTotalReconnectTries(int reconnectTries);
    int totalReconnectTries() const;

    void setUserAgent(const std::string& userAgent);
    const std::string& userAgent() const;

    void setProxyCredentials(const Credentials& credentials);
    const Credentials& proxyCredentials() const;

    void setProxyVia(const SocketAddress& proxyEndpoint, bool isSecure, ssl::AdapterFunc adapterFunc);
    const std::optional<SocketAddress>& proxyEndpoint() const;
    bool isProxySecure() const;
    const ssl::AdapterFunc& proxyAdapterFunc() const;

    /** If set to true client will not try to add Authorization header to the first request. false by default. */
    void setDisablePrecalculatedAuthorization(bool val);
    bool disablePrecalculatedAuthorization() const;

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

    void setTimeouts(const Timeouts& timeouts);
    const Timeouts& timeouts() const;

    void addAdditionalHeader(const std::string& key, const std::string& value);
    void addRequestHeaders(const HttpHeaders& headers);
    void removeAdditionalHeader(const std::string& key);
    void setAdditionalHeaders(HttpHeaders additionalHeaders);
    const HttpHeaders& additionalHeaders() const;

    void setMaxNumberOfRedirects(int maxNumberOfRedirects);
    int maxNumberOfRedirects() const;

    void setAuthType(AuthType value);
    AuthType authType() const;

    void setCredentials(const Credentials& credentials);
    const Credentials& credentials() const;

    void assignOptions(const ClientOptions& other);

private:
    bool m_contentEncodingUsed = true;
    int m_subsequentReconnectTries = 1;
    int m_totalReconnectTries = 1;
    std::string m_userAgent;
    Credentials m_credentials;
    Credentials m_proxyCredentials;
    std::optional<SocketAddress> m_proxyEndpoint;
    ssl::AdapterFunc m_proxyAdapterFunc = ssl::kDefaultCertificateCheck;
    bool m_isProxySecure = false;
    Timeouts m_timeouts;
    AuthType m_authType = AuthType::authBasicAndDigest;
    HttpHeaders m_additionalHeaders;
    bool m_precalculatedAuthorizationDisabled = false;
    int m_maxNumberOfRedirects = 5;
};

//-------------------------------------------------------------------------------------------------

/**
 * HTTP client. All operations are done asynchronously.
 *
 * All events (setOn...) are delivered within object's aio thread.
 * State is changed just before delivering event.
 * This class methods are not thread-safe.
 * NOTE: This class is a replacement for nx::network::http::AsyncHttpClient.
 * WARNING: It is strongly recommended to listen for someMessageBodyAvailable() event and
 *   read current message body buffer with AsyncClient::fetchMessageBodyBuffer() call every time
 *   to avoid internal message body buffer consuming too much memory.
 */
class NX_NETWORK_API AsyncClient:
    public nx::network::aio::BasicPollable,
    public ClientOptions
{
    using base_type = nx::network::aio::BasicPollable;

public:
    using CustomRequestPrepareFunc = nx::utils::MoveOnlyFunc<void(Request* request)>;

    static constexpr int UNLIMITED_RECONNECT_TRIES = -1;

    AsyncClient(ssl::AdapterFunc adapterFunc);

    /**
     * Set already connected socket to force it for the very first request.
     */
    AsyncClient(std::unique_ptr<AbstractStreamSocket> socket, ssl::AdapterFunc adapterFunc);

    virtual ~AsyncClient() override;

    AsyncClient(const AsyncClient&) = delete;
    AsyncClient& operator=(const AsyncClient&) = delete;

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    /**
     * @return true if no response has been received due to transport error.
     */
    bool failed() const;
    SystemError::ErrorCode lastSysErrorCode() const;

    /**
     * Invoked after request has been sent.
     * @param isRetryAfterUnauthorized set to true if request has been sent as after receiving 401 unauthorized response.
     * TODO: #akolesnikov Check if really needed.
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
     * NOTE: Some message body can still be stored in internal buffer.
     *   To read it, call AsyncClient::fetchMessageBodyBuffer.
     */
    void setOnDone(nx::utils::MoveOnlyFunc<void()> handler);

    void setRequestBody(std::unique_ptr<AbstractMsgBodySource> body);

    /**
     * Start GET request to url.
     * @return true, if socket is created and async connect is started. false otherwise.
     *   To get error description use SystemError::getLastOSErrorCode().
     */
    void doGet(const nx::utils::Url& url);

    /**
     * This overload is same as:
     * @code{.cpp}
     * httpClient->setOnDone(func);
     * httpClient->doGet(url);
     * @endcode
     */
    void doGet(
        const nx::utils::Url& url,
        nx::utils::MoveOnlyFunc<void()> completionHandler);

    void doHead(const nx::utils::Url& url);
    void doHead(
        const nx::utils::Url& url,
        nx::utils::MoveOnlyFunc<void()> completionHandler);

    /**
     * Start POST request to url.
     * @return true, if socket is created and async connect is started. false otherwise
     */
    void doPost(const nx::utils::Url& url);
    void doPost(
        const nx::utils::Url& url,
        std::unique_ptr<AbstractMsgBodySource> body);
    void doPost(
        const nx::utils::Url& url,
        std::unique_ptr<AbstractMsgBodySource> body,
        nx::utils::MoveOnlyFunc<void()> completionHandler);
    void doPost(
        const nx::utils::Url& url,
        nx::utils::MoveOnlyFunc<void()> completionHandler);

    void doPut(const nx::utils::Url& url);
    void doPut(
        const nx::utils::Url& url,
        std::unique_ptr<AbstractMsgBodySource> body);
    void doPut(
        const nx::utils::Url& url,
        std::unique_ptr<AbstractMsgBodySource> body,
        nx::utils::MoveOnlyFunc<void()> completionHandler);
    void doPut(
        const nx::utils::Url& url,
        nx::utils::MoveOnlyFunc<void()> completionHandler);

    void doPatch(const nx::utils::Url& url);
    void doPatch(
        const nx::utils::Url& url,
        std::unique_ptr<AbstractMsgBodySource> body);
    void doPatch(
        const nx::utils::Url& url,
        std::unique_ptr<AbstractMsgBodySource> body,
        nx::utils::MoveOnlyFunc<void()> completionHandler);
    void doPatch(
        const nx::utils::Url& url,
        nx::utils::MoveOnlyFunc<void()> completionHandler);

    void doDelete(const nx::utils::Url& url);
    void doDelete(
        const nx::utils::Url& url,
        nx::utils::MoveOnlyFunc<void()> completionHandler);

    void doUpgrade(
        const nx::utils::Url& url,
        const std::string& protocolToUpgradeTo);
    void doUpgrade(
        const nx::utils::Url& url,
        const Method& method,
        const std::string& protocolToUpgradeTo);

    void doUpgrade(
        const nx::utils::Url& url,
        const std::string& protocolToUpgradeTo,
        nx::utils::MoveOnlyFunc<void()> completionHandler);
    void doUpgrade(
        const nx::utils::Url& url,
        const Method& method,
        const std::string& protocolToUpgradeTo,
        nx::utils::MoveOnlyFunc<void()> completionHandler);

    void doConnect(
        const nx::utils::Url& proxyUrl,
        const std::string& targetHost);
    void doConnect(
        const nx::utils::Url& proxyUrl,
        const std::string& targetHost,
        nx::utils::MoveOnlyFunc<void()> completionHandler);

    void doRequest(
        const Method& method,
        const nx::utils::Url& url);
    void doRequest(
        const Method& method,
        const nx::utils::Url& url,
        nx::utils::MoveOnlyFunc<void()> completionHandler);

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

    /**
     * Can be invoked after receiving response only.
     */
    std::unique_ptr<AbstractMsgBodySource> takeResponseBodySource();

    const nx::utils::Url& url() const;
    const nx::utils::Url& contentLocationUrl() const;

    /**
     * Number of bytes read (including http request line and headers)
     *  via single HTTP request.
     */
    quint64 bytesRead() const;

    const std::unique_ptr<AbstractStreamSocket>& socket();

    /**
     * Returns socket in non-blocking mode.
     * NOTE: Can be called within object's aio thread only.
     */
    std::unique_ptr<AbstractStreamSocket> takeSocket();

    /**
     * @param func Invoked after fully preparing request to be sent.
     */
    void setCustomRequestPrepareFunc(CustomRequestPrepareFunc func);
    void setKeepAlive(const KeepAliveOptions& keepAliveOptions);

    static std::string endpointWithProtocol(const nx::utils::Url& url);

    int totalRequestsSentViaCurrentConnection() const;
    int totalRequestsSent() const;

    /**
     * After this function has been called no new socket::read() will be invoked by HttpClient
     * until HttpClient::resumeReading() is called.
     * NOTE: Should be called only within the same AIO thread with the HttpClient socket.
     */
    void stopReading();

    /**
     * Resume reading stopped by the HttpClient::stopReading() function. If
     * HttpClient::stopReading() hasn't been called this function will have no effect.
     * NOTE: Should be called only within the same AIO thread with the HttpClient socket.
     */
    void resumeReading();
    bool isReading();

    ssl::AdapterFunc setAdapterFunc(ssl::AdapterFunc adapterFunc);
    const ssl::AdapterFunc& adapterFunc() const;

    static void setForceTrafficLogging(bool value);

private:
    enum class State
    {
        sInit,
        sWaitingConnectToHost,
        sSendingRequest,
        sReceivingResponse,
        sResponseReceived,
        sReadingMessageBody,
        sFailed,
        sDone,
    };

    enum class Result
    {
        thisDestroyed = 0,
        cancelled,
        newRequestScheduled,
        proceed,
    };

    enum class ConnectionReusePolicy
    {
        noReuse,
        establishedPipeline,
        rawConnection,
    };

    nx::utils::MoveOnlyFunc<void(bool /*isRetryAfterUnauthorizedResponse*/)> m_onRequestHasBeenSent;
    nx::utils::MoveOnlyFunc<void()> m_onResponseReceived;
    nx::utils::MoveOnlyFunc<void()> m_onSomeMessageBodyAvailable;
    nx::utils::MoveOnlyFunc<void()> m_onDone;

    State m_state = State::sInit;
    Request m_request;
    Message m_response;
    nx::Buffer m_responseMessageBody;
    std::unique_ptr<AbstractStreamSocket> m_socket;
    std::unique_ptr<AsyncMessagePipeline> m_messagePipeline;
    nx::utils::Url m_requestUrl;
    nx::utils::Url m_contentLocationUrl;
    bool m_authorizationTried = false;
    bool m_proxyAuthorizationTried = false;
    bool m_ha1RecalcTried = false;
    int m_totalRequestsSentViaCurrentConnection = 0;
    int m_totalRequestsSent = 0;
    int m_totalRequestsScheduled = 0;
    CustomRequestPrepareFunc m_customRequestPrepareFunc;
    int m_messageReceivedThroughTheCurrentConnectionCount = 0;
    int m_currentMessageNumber = 0;
    int m_awaitedMessageNumber = 0;
    int m_lastReportedMessageNumber = -1;
    std::string m_remoteEndpointWithProtocol;
    SystemError::ErrorCode m_lastSysErrorCode = SystemError::noError;
    int m_requestSequence = 0;
    int m_numberOfRedirectsTried = 0;
    nx::utils::InterruptionFlag m_objectDestructionFlag;
    std::unique_ptr<AbstractMsgBodySource> m_requestBody;
    bool m_readingCeased = false;
    bool m_isPersistentConnection = false;
    int m_closeHandlerId = -1;
    ssl::AdapterFunc m_adapterFunc;
    std::optional<KeepAliveOptions> m_keepAliveOptions;

    virtual void stopWhileInAioThread() override;

    void reportConnectionFailure(SystemError::ErrorCode errorCode);
    void sendRequest();
    void asyncConnectDone(SystemError::ErrorCode errorCode);
    void onRequestSent(SystemError::ErrorCode errorCode);

    void initializeMessagePipeline();
    void onMessageReceived(Message message);
    void onSomeMessageBodyAvailable(nx::Buffer buffer);
    void onMessageEnd();
    void onConnectionClosed(SystemError::ErrorCode reason);

    void resetDataBeforeNewRequest();
    void initiateHttpMessageDelivery();
    ConnectionReusePolicy getConnectionReusePolicy() const;
    void sendRequestOverExternalConnection();
    bool configureSocket(AbstractStreamSocket* connection);
    void initiateTcpConnection();
    bool isMalformed(const nx::network::http::Response& response) const;
    bool repeatRequestIfNeeded(const Response& response);
    bool sendRequestToNewLocation(const Response& response);
    void composeRequest(const Method& httpMethod);
    void prepareRequestLine(bool useHttp11, const Method& httpMethod);
    void prepareRequestHeaders(bool useHttp11, const Method& httpMethod);
    void addAppropriateAuthenticationInformation();
    void addBodyToRequest();
    bool isIgnoringCurrentMessage() const;

    /**
     * @return true, if connected.
     */
    bool reconnectIfAppropriate();

    /** Composes request with authorization header based on response. */
    bool resendRequestWithAuthorization(
        const nx::network::http::Response& response,
        bool isProxy = false);

    bool addDigestAuthorizationToRequest(
        const SocketAddress& serverEndpoint,
        server::Role serverRole,
        const Credentials& credentials,
        const header::WWWAuthenticate& authenticateHeader,
        const std::string_view& authorizationHeaderName);

    void addBasicAuthorizationToRequest();

    void doSomeCustomLogic(
        const nx::network::http::Response& response,
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

} // namespace nx::network::http
