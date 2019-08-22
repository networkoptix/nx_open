#pragma once

#include <optional>

#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/utils/thread/mutex.h>

#include <QtCore/QElapsedTimer>
#include <nx/utils/timer_manager.h>

namespace nx {
namespace network {
namespace http {

/**
 * A pool of http connections.
 * rest::ServerConnection sends requests through this pool using ClientPool::sendRequest(...)
 * and catching responses from event ClientPool::void done(...)
 */
class ClientPool: public QObject
{
    Q_OBJECT
public:

    /**
     * Wraps request data and intermediate values.
     * TODO: Should be merged with nx::network::http::Request later.
     */
    struct Request
    {
        Request():
            authType(nx::network::http::AuthType::authBasicAndDigest)
        {
        }

        bool isValid() const
        {
            return !method.isEmpty() && url.isValid();
        }

        Method::ValueType method;
        nx::utils::Url url;
        nx::network::http::HttpHeaders headers;
        nx::network::http::StringType contentType;
        nx::network::http::StringType messageBody;
        nx::network::http::AuthType authType;
        std::optional<std::chrono::milliseconds> timeout{std::nullopt};
    };

    // TODO: Should be merged with nx::network::http::Response
    struct Response
    {
        SystemError::ErrorCode systemError = SystemError::noError;
        nx::network::http::StatusLine statusLine;
        nx::network::http::StringType contentType;
        nx::network::http::HttpHeaders headers;
        nx::network::http::BufferType messageBody;

        /** Clean up all internal values to default state. */
        void reset();
        // TODO: Here we can add more parsers
    };

    using HttpCompletionFunc = std::function<void (
        int handle,
        SystemError::ErrorCode errorCode,
        int statusCode,
        nx::network::http::StringType contentType,
        nx::network::http::BufferType msgBody,
        const nx::network::http::HttpHeaders& headers)>;

    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    /**
     * Wraps up all the data for request, response and all intermediate routines.
     * Context exists all the time from request creation to the point response is handled.
     * Its data can be accessed from following threads:
     *  - thread for ClientPool::sendRequest
     *  - AioThread, at a callback at_HttpClientDone
     */
    struct Context
    {
        virtual ~Context() = default;

        enum class State
        {
            initial,
            sending,
            waitingResponse,
            /** Got an error in RestResponse */
            hasResponse,
            /** Failed to get any response due to network error. */
            noResponse,
        };

        State state = State::initial;
        Request request;
        Response response;
        /** Time when request was sent to AsyncHttpClientPtr. */
        TimePoint stampSent;
        /** Time when response was received by ClientPool. */
        TimePoint stampReceived;
        /** Handle of request being served by this connection right now. */
        int handle = 0;
        /** Callback to be called when response is received. */
        HttpCompletionFunc completionFunc;

        std::optional<QPointer<QThread>> targetThread;

        mutable QnMutex mutex;

        bool hasResponse() const;
        StatusCode::Value getStatusCode() const;
        nx::utils::Url getUrl() const;
    };
    using ContextPtr = QSharedPointer<Context>;

    ClientPool(QObject *parent = nullptr);
    virtual ~ClientPool();

    /**
     * doGet and doPost are used by the chain QnSessionManager->QnAbstractConnection->QnMediaServerConnection
     * These methods will become obsolete when we finish QnMediaServerConnection refactoring.
     */
    int doGet(
        const nx::utils::Url& url,
        nx::network::http::HttpHeaders headers = nx::network::http::HttpHeaders(),
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    int doPost(
        const nx::utils::Url& url,
        const QByteArray& contentType,
        const QByteArray& msgBody,
        nx::network::http::HttpHeaders headers = nx::network::http::HttpHeaders(),
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    /**
     * It is used to run all the requests from rest::ServerConnection.
     */
    int sendRequest(ContextPtr request);

    void terminate(int handle);
    void setPoolSize(int value);

    struct RequestStats
    {
        int total = 0;
        int sent = 0;
        int queued = 0;
        int connections = 0;
    };

    RequestStats getRequestStats() const;

    /** Returns amount of requests which are running or awaiting to be run */
    int size() const;

    void setDefaultTimeouts(
        std::chrono::milliseconds request,
        std::chrono::milliseconds response,
        std::chrono::milliseconds messageBody);

    /** Fills in response data to Context, using response from httpClient. */
    static void readHttpResponse(Context& context, AsyncHttpClientPtr httpClient);

signals:
    void requestIsDone(QSharedPointer<Context> context);

private slots:
    void at_HttpClientDone(AsyncHttpClientPtr httpClient);

private:
    AsyncHttpClientPtr createHttpConnection();

    /**
     * Contains http connection and data for active request.
     * HttpConnection instance is reused after request is complete.
     */
    struct HttpConnection;

private:
    HttpConnection* getUnusedConnection(const nx::utils::Url &url);
    void sendRequestUnsafe(ContextPtr context, AsyncHttpClientPtr httpClient);
    void sendNextRequestUnsafe();
    void cleanupDisconnectedUnsafe();

private:
    mutable QnMutex m_mutex;
    typedef std::unique_ptr<HttpConnection> HttpConnectionPtr;
    std::multimap<QString /*endpointWithProtocol*/, HttpConnectionPtr> m_connectionPool;

    /** A sequence of requests to be passed to connection pool to process. */
    std::map<int, ContextPtr> m_awaitingRequests;
    int m_maxPoolSize;
    int m_requestId;
    std::chrono::milliseconds m_defaultRequestTimeout{std::chrono::minutes(1)};
    std::chrono::milliseconds m_defaultResponseTimeout{std::chrono::minutes(1)};
    std::chrono::milliseconds m_defaultMessageBodyTimeout{std::chrono::minutes(1)};
};

} // namespace nx
} // namespace network
} // namespace http
