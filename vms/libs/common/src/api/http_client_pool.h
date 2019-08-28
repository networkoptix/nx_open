#pragma once

#include <optional>
#include <QtCore/QElapsedTimer>
#include <QtCore/QSharedPointer>
#include <QtCore/QPointer>

#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/utils/thread/mutex.h>
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
    };

    // TODO: Should be merged with nx::network::http::Response
    struct Response
    {
        nx::network::http::StatusLine statusLine;
        nx::network::http::StringType contentType;
        nx::network::http::HttpHeaders headers;
        nx::network::http::BufferType messageBody;

        /** Clean up all internal values to default state. */
        void reset();
    };

    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    /**
     * Wraps up all the data for request, response and all intermediate routines.
     * Context exists all the time from request creation to the point response is handled.
     * Its data can be accessed from following threads:
     *  - thread for ClientPool::sendRequest. It can be main thread, or feature thread
     *  - AioThread, at a callback at_HttpClientDone
     *  - AioThread at final callback, if no `targetThread` is specified
     *  - `targetThread` if it is specified
     */
    class Context
    {
    public:
        using milliseconds = std::chrono::milliseconds;

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

        Request request;
        Response response;
        /** Time when request was sent to AsyncHttpClientPtr. */
        TimePoint stampSent;
        /** Time when response was received by ClientPool. */
        TimePoint stampReceived;

        SystemError::ErrorCode systemError = SystemError::noError;

        std::optional<std::chrono::milliseconds> timeout{std::nullopt};
        /** Handle of request being served by this connection right now. */
        int handle = 0;
        /** Callback to be called when response is received. */
        std::function<void (QSharedPointer<Context> context)> completionFunc;

        QThread* getTargetThread() const;
        /** Set thread for dispatched callback. */
        void setTargetThread(QThread* thread);
        /** Checks if target thread is dead. */
        bool targetThreadIsDead() const;
        /** Check if there is any response. */
        bool hasResponse() const;
        /** Get HTTP status code of a response. */
        StatusCode::Value getStatusCode() const;
        /** Get request URL. */
        nx::utils::Url getUrl() const;

        /**
         * Checks if request is complete.
         * It can mean:
         *  - got a response
         *  - got an error sending request
         *  - any other error
         */
        bool isFinished() const;

        /**
         * Get total time for request.
         * It measures time between sending request and receiving full response.
         */
        milliseconds getTimeElapsed() const;

        /** Sends request using httpClient. */
        void sendRequest(
            AsyncHttpClientPtr httpClient,
            milliseconds responseTimeout,
            milliseconds messageBodyTimeout);

        /** Fills in response data to Context, using response from httpClient. */
        void readHttpResponse(AsyncHttpClientPtr httpClient);

    protected:
        State state = State::initial;
        mutable QnMutex mutex;
        std::optional<QPointer<QThread>> targetThread;
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

    /** Internal statistics for debug output. */
    struct RequestStats
    {
        int total = 0; /**< Number of queued requests and number of active connections. */
        int sent = 0; /**< Number of active requests. */
        int queued = 0; /**< Number of requests in queue. */
        int connections = 0; /**< Number of connections. */
    };

    /** Read internal statistics. */
    RequestStats getRequestStats() const;

    /** Returns amount of requests which are running or awaiting to be run .*/
    int size() const;

    void setDefaultTimeouts(
        std::chrono::milliseconds request,
        std::chrono::milliseconds response,
        std::chrono::milliseconds messageBody);

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

Q_DECLARE_METATYPE(nx::network::http::ClientPool::ContextPtr);
