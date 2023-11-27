// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QPointer>
#include <QtCore/QSharedPointer>

#include <nx/network/http/http_async_client.h>
#include <nx/string.h>
#include <nx/utils/impl_ptr.h>

namespace nx::network::http {

/**
 * A pool of http connections, effectively limiting number of simultaneous requests.
 */
class NX_VMS_COMMON_API ClientPool: public QObject
{
    Q_OBJECT
public:
    /**
     * Wraps request data and intermediate values.
     * TODO: Should be merged with nx::network::http::Request later.
     */
    struct NX_VMS_COMMON_API Request
    {
        Request():
            authType(nx::network::http::AuthType::authBasicAndDigest)
        {
        }

        bool isValid() const
        {
            return !method.toString().empty() && url.isValid();
        }

        Method method;
        nx::utils::Url url;
        nx::network::http::HttpHeaders headers;
        nx::String contentType;
        nx::String messageBody;
        nx::network::http::AuthType authType;
        std::optional<nx::network::http::Credentials> credentials;

        /** Server id if request is being proxied through another server. */
        std::optional<QnUuid> gatewayId;
    };

    // TODO: Should be merged with nx::network::http::Response
    struct NX_VMS_COMMON_API Response
    {
        nx::network::http::StatusLine statusLine;
        nx::String contentType;
        nx::network::http::HttpHeaders headers;
        QByteArray messageBody;

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
     *  - AioThread, at a callback onHttpClientDone
     *  - AioThread at final callback, if no `targetThread` is specified
     *  - `targetThread` if it is specified
     */
    class NX_VMS_COMMON_API Context
    {
    public:
        using milliseconds = std::chrono::milliseconds;

        Context(const QnUuid& adapterFuncId, ssl::AdapterFunc adapterFunc):
            adapterFuncId(adapterFuncId), adapterFunc(std::move(adapterFunc))
        {
        }

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
            /** Request was canceled by client. */
            canceled,
        };

        QnUuid adapterFuncId;
        ssl::AdapterFunc adapterFunc;
        Request request;
        Response response;

        /** Time when request was sent to AsyncClient. */
        TimePoint stampSent;

        /** Time when response was received by ClientPool. */
        TimePoint stampReceived;

        SystemError::ErrorCode systemError = SystemError::noError;

        /** Custom request timeouts. */
        std::optional<AsyncClient::Timeouts> timeouts;

        /** Handle of request being served by this connection right now. */
        int handle = 0;

        /** Callback to be called when response is received. */
        std::function<void (QSharedPointer<Context> context)> completionFunc;

        QThread* targetThread() const;

        /** Set thread for dispatched callback. */
        void setTargetThread(QThread* thread);

        /** Checks if target thread is dead. */
        bool targetThreadIsDead() const;

        /** Check if there is any response. */
        bool hasResponse() const;

        /** Get HTTP status of a response. */
        StatusLine getStatusLine() const;

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
         * Checks if request was canceled.
         * It happens only when client calls ClientPool::terminate(...) for this request.
         */
        bool isCanceled() const;

        /** Whether request completed successfully. */
        bool hasSuccessfulResponse() const;

        /**
         * Get total time for request.
         * It measures time between sending request and receiving full response.
         */
        milliseconds getTimeElapsed() const;

        /**
         * Sends request using httpClient.
         * It called by ClientPool. It passes its default timeout values here. Context
         * can use its own timeout value if it is not empty.
         * @param httpClient - http client to be used
         */
        void sendRequest(AsyncClient* httpClient);

        /**
         * Marks request as canceled.
         * It just sets internal state to 'canceled'. It is called by ClientPool.
         */
        void setCanceled();

        /** Fills in response data to Context, using response from httpClient. */
        void readHttpResponse(AsyncClient* httpClient);

    protected:
        State state = State::initial;
        mutable nx::Mutex mutex;
        /**
         * Target thread for callback.
         * Empty value makes dispatcher invoke callback in AIO thread.
         */
        std::optional<QPointer<QThread>> thread;
    };
    using ContextPtr = QSharedPointer<Context>;

    ClientPool(QObject *parent = nullptr);
    virtual ~ClientPool();

public:
    /**
     * It is used to run all the requests from rest::ServerConnection.
     */
    int sendRequest(ContextPtr request);

    bool terminate(int handle);
    void setPoolSize(int value);

    void stop(bool invokeCallbacks = true);

    /** Returns amount of requests which are running or awaiting to be run .*/
    int size() const;

    AsyncClient::Timeouts defaultTimeouts() const;
    void setDefaultTimeouts(AsyncClient::Timeouts timeouts);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::network::http
