#include <functional>
#include <memory>
#include <mutex>
#include <stdio.h>
#include <thread>

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

    void startReading()
    {
        http::HttpHeaders websocketHeaders;
        websocket::addClientHeaders(&websocketHeaders, config.protocolName);

        LOG_VERBOSE("Connecting to %s", config.url.constData());

        m_httpClient.setUserName(config.userName);
        m_httpClient.setUserPassword(config.userPassword);
        m_httpClient.addRequestHeaders(websocketHeaders);
        m_httpClient.doGet(config.url,
            [self = shared_from_this(), this]()
            {
                if (m_stopped)
                {
                    LOG_VERBOSE("onConnect: seems like connection has been destroyed, exiting");
                    return;
                }

                if (self->m_httpClient.state() == nx::network::http::AsyncClient::State::sFailed
                    || !m_httpClient.response())
                {
                    LOG_INFO("onConnect: Http client failed to connect to host");
                    stopInAioThread();
                    return;
                }

                const int statusCode = m_httpClient.response()->statusLine.statusCode;
                LOG_VERBOSE("onConnect: Http client status code: %d", statusCode);

                if (!nx::network::http::StatusCode::isSuccessCode(statusCode))
                {
                    LOG_INFO("onConnect: Http client got invalid response code %d", statusCode);
                    stopInAioThread();
                    return;
                }

                LOG_VERBOSE("onConnect: HTTP client connected successfully to the %s",
                    config.url.constData());

                auto validationError = websocket::validateResponse(m_httpClient.request(),
                    *m_httpClient.response());
                if (validationError != websocket::Error::noError)
                {
                    LOG_INFO("onConnect: Websocket handshake validation error: %d", validationError);
                    stopInAioThread();
                    return;
                }

                LOG_VERBOSE("onConnect: Websocket handshake response validated successfully");
                m_websocket.reset(new websocket::WebSocket(m_httpClient.takeSocket(),
                    websocket::FrameType::text));
                m_websocket->bindToAioThread(m_aioThread);

                m_httpClient.pleaseStopSync();

                m_websocket->start();
                m_websocket->readSomeAsync(&m_readBuffer,
                    [self, this](SystemError::ErrorCode errorCode, size_t bytesRead)
                    { onRead(errorCode, bytesRead); });
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
    nx::Buffer m_readBuffer;

    void stopInAioThread()
    {
        m_stopped = true;
        m_readyPromise.set_value();
    }

    void onRead(SystemError::ErrorCode errorCode, size_t bytesRead)
    {
        if (m_stopped)
        {
            LOG_VERBOSE("onRead: seems like connection has been destroyed, exiting");
            return;
        }

        if (errorCode != SystemError::noError)
        {
            LOG_INFO("onRead: read failed with error code: %d", errorCode);
            stopInAioThread();
            return;
        }

        if (bytesRead == 0)
        {
            LOG_INFO("onRead: connection has been closed by remote peer");
            stopInAioThread();
            return;
        }

        LOG_VERBOSE("onRead: successfully read message: %s", m_readBuffer.constData());
        m_readBuffer = nx::Buffer();

        m_websocket->readSomeAsync(&m_readBuffer,
            [self = shared_from_this(), this](SystemError::ErrorCode errorCode, size_t bytesRead)
            { onRead(errorCode, bytesRead); });
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

static void connectAndListen()
{
    auto websocketConnection = std::make_shared<WebsocketConnection>(config.aioThread);
    websocketConnectionsPoolInstance->addConnection(websocketConnection);
    websocketConnection->startReading();
}

void prepareConfig(int argc, const char* argv[])
{
    config.protocolName = "nxp2p";
    config.url = "ws://127.0.0.1:7001/ec2/transactionBus";
    config.userName = "admin";
    config.userPassword = "admin";
    config.role = Role::client;
    config.logLevel = verbose;
}

#if defined(_WIN32)
BOOL ctrlHandler(DWORD fdwCtrlType)
{
    websocketConnectionsPoolInstance->stopAll();
    return true;
}
#endif // _WIN32

int main(int argc, const char *argv[])
{
    WebsocketConnectionsPool websocketConnectionsPool;
    websocketConnectionsPoolInstance = &websocketConnectionsPool;

    #if defined(_WIN32)
        if (!SetConsoleCtrlHandler(ctrlHandler, true))
            return -1;
    #endif

    auto sgGuard = std::make_unique<SocketGlobalsHolder>(0);
    prepareConfig(argc, argv);
    aio::Timer timer;
    config.aioThread = timer.getAioThread();

    setvbuf(stdout, nullptr, _IONBF, 0);

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
}
