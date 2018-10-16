#include <functional>
#include <memory>
#include <mutex>
#include <stdio.h>

#include <signal.h>

#include <nx/network/aio/timer.h>
#include <nx/network/websocket/websocket.h>
#include <nx/network/websocket/websocket_handshake.h>
#include <nx/network/http/http_async_client.h>
#include <nx/utils/std/future.h>
#include <nx/utils/move_only_func.h>

using namespace nx::network;

enum class Role
{
    server,
    client
};

enum LogLevel
{
    info,
    verbose
};

struct
{
    Role role;
    nx::Buffer protocolName;
    nx::Buffer url;
    nx::Buffer userName;
    nx::Buffer userPassword;
    nx::Buffer serverAddress;
    int serverPort;
    aio::AbstractAioThread* aioThread = nullptr;
    LogLevel logLevel;
} config;

#define LOG_INFO(fmt, ...) \
    do { \
        if (config.logLevel >= LogLevel::info) \
            fprintf(stdout, fmt "\n" , ##__VA_ARGS__); \
    } while (0)

#define LOG_VERBOSE(fmt, ...) \
    do { \
        if (config.logLevel >= LogLevel::verbose) \
            fprintf(stdout, fmt "\n" , ##__VA_ARGS__); \
    } while (0)

class WebsocketConnectionsPool;
static WebsocketConnectionsPool* websocketConnectionsPoolInstance;

static void startAccepting()
{
}

class WebsocketConnection;
using WebsocketConnectionPtr = std::shared_ptr<WebsocketConnection>;

class WebsocketConnection: public std::enable_shared_from_this<WebsocketConnection>
{
public:
    WebsocketConnection(aio::AbstractAioThread* aioThread): m_aioThread(aioThread)
    {
        m_httpClient.bindToAioThread(aioThread);
    }

    using CompletionHandler = nx::utils::MoveOnlyFunc<void(const WebsocketConnectionPtr&)>;
    void connectAsync(const nx::Buffer& url, const nx::Buffer& protocol,
        CompletionHandler onConnect)
    {
        http::HttpHeaders websocketHeaders;
        websocket::addClientHeaders(&websocketHeaders, protocol);

        LOG_VERBOSE("Connecting to %s", url.constData());
        m_httpClient.doGet(url,
            [self = shared_from_this(), this]()
            {
                if (m_stopped)
                    return;

                if (self->m_httpClient.state() == nx::network::http::AsyncClient::State::sFailed
                    || !m_httpClient.response())
                {
                    LOG_INFO("Http client failed to connect to host");
                    stopInAioThread();
                    return;
                }

                const int statusCode = m_httpClient.response()->statusLine.statusCode;
                LOG_VERBOSE("Http client status code: %d", statusCode);

                if (!nx::network::http::StatusCode::isSuccessCode(statusCode))
                {
                    LOG_INFO("Http client got invalid response code %d", statusCode);
                    stopInAioThread();
                    return;
                }
            });
    }

    void stop()
    {
        m_aioThread->post(nullptr, [this]() { stopInAioThread(); });
    }

    void waitForDone()
    {
        m_readyFuture.wait();
    }

private:
    http::AsyncClient m_httpClient;
    WebSocketPtr m_websocket;
    aio::AbstractAioThread* m_aioThread = nullptr;
    bool m_stopped = false;
    nx::utils::promise<void> m_readyPromise;
    nx::utils::future<void> m_readyFuture = m_readyPromise.get_future();

    void stopInAioThread()
    {
        m_stopped = true;
        m_readyPromise.set_value();
    }
};


class WebsocketConnectionsPool
{
public:
    void addConnection(const WebsocketConnectionPtr websocketConnection)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopped)
        {
            websocketConnection->stop();
            return;
        }

        m_connections.insert(websocketConnection);
    }

    void stopAll()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopped)
            return;

        m_stopped = true;
        for (const auto& connection: m_connections)
            connection->stop();
    }

    void waitAll()
    {
        for (const auto& connection: m_connections)
            connection->waitForDone();
    }

private:
    std::mutex m_mutex;
    bool m_stopped = false;
    std::set<WebsocketConnectionPtr> m_connections;
};

static void onConnect(WebsocketConnectionPtr websocketConnection)
{
}

static void connectAndListen()
{
    auto websocketConnection = std::make_shared<WebsocketConnection>(config.aioThread);
    websocketConnectionsPoolInstance->addConnection(websocketConnection);
    websocketConnection->connectAsync(config.url, config.protocolName, onConnect);
}

void prepareConfig(int argc, const char* argv[])
{
    config.protocolName = "nxp2p";
    config.url = "ws://127.0.0.1:7001/ec2/transactionBus";
}

int main(int argc, const char *argv[])
{
    prepareConfig(argc, argv);
    aio::Timer timer;
    config.aioThread = timer.getAioThread();

    setvbuf(stdout, nullptr, _IONBF, 0);

    WebsocketConnectionsPool websocketConnectionsPool;
    websocketConnectionsPoolInstance = &websocketConnectionsPool;

    switch (config.role)
    {
        case Role::server:
            startAccepting();
            break;
        case Role::client:
            connectAndListen();
            break;
    }

    websocketConnectionsPool.waitAll();

    return 0;
    //nx::utils::promise<void> readyPromise;
    //auto readyFuture = readyPromise.get_future();

    //http::AsyncClient httpClient;
    //http::HttpHeaders headers;
    //websocket::addClientHeaders(&headers, "nxp2p");
    //httpClient.setAdditionalHeaders(headers);

    //httpClient.doGet("http://admin:admin@127.0.0.1:7001/ec2/transactionBus",
    //    [&readyPromise, &httpClient]()
    //    {
    //        if (httpClient.state() == nx::network::http::AsyncClient::State::sFailed
    //            || !httpClient.response())
    //        {
    //            std::cout << "http client failed" << std::endl;
    //            readyPromise.set_value();
    //            return;
    //        }

    //        const int statusCode = m_httpClient->response()->statusLine.statusCode;

    //        NX_VERBOSE(this, lit("%1. statusCode = %2").arg(Q_FUNC_INFO).arg(statusCode));

    //        if (statusCode == nx::network::http::StatusCode::unauthorized)
    //        {
    //            // try next credential source
    //            m_credentialsSource = (CredentialsSource)((int)m_credentialsSource + 1);
    //            if (m_credentialsSource < CredentialsSource::none)
    //            {
    //                using namespace std::placeholders;
    //                fillAuthInfo(m_httpClient.get(), m_credentialsSource == CredentialsSource::serverKey);
    //                m_httpClient->doGet(
    //                    m_httpClient->url(),
    //                    std::bind(&ConnectionBase::onHttpClientDone, this));
    //            }
    //            else
    //            {
    //                cancelConnecting(State::Unauthorized, lm("Unauthorized"));
    //            }
    //            return;
    //        }
    //        else if (!nx::network::http::StatusCode::isSuccessCode(statusCode)) //< Checking that statusCode is 2xx.
    //        {
    //            cancelConnecting(State::Error, lm("Not success HTTP status code %1").arg(statusCode));
    //            return;
    //        }
    //    });

    //websocket::WebSocket webSocket;

    //readyPromise.wait();
}
