#include <functional>
#include <memory>
#include <mutex>
#include <stdio.h>
#include <thread>
#include <string.h>
#include <stdlib.h>

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

enum class LogLevel
{
    info,
    verbose
};

struct
{
    Role role = Role::client;
    nx::Buffer protocolName = "nxp2p";
    nx::Buffer url;
    nx::Buffer userName;
    nx::Buffer userPassword;
    nx::Buffer serverAddress = "0.0.0.0";
    int serverPort;
    aio::AbstractAioThread* aioThread = nullptr;
    LogLevel logLevel = LogLevel::info;
} config;

#define LOG_INFO(fmt, ...) \
    do { \
        if (config.logLevel >= LogLevel::info) \
            fprintf(stdout, "%s: " fmt "\n", __func__,  ##__VA_ARGS__); \
    } while (0)

#define LOG_VERBOSE(fmt, ...) \
    do { \
        if (config.logLevel >= LogLevel::verbose) \
            fprintf(stdout, "%s: " fmt "\n", __func__,  ##__VA_ARGS__); \
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

        LOG_VERBOSE("connecting to %s", config.url.constData());

        m_httpClient.setUserName(config.userName);
        m_httpClient.setUserPassword(config.userPassword);
        m_httpClient.addRequestHeaders(websocketHeaders);
        m_httpClient.doGet(config.url, [self = shared_from_this(), this]() { onConnect(); });
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
        LOG_VERBOSE("connection: stop requested, shutting down");
        m_stopped = true;
        m_readyPromise.set_value();
    }

    void onConnect()
    {
        if (m_stopped)
        {
            LOG_VERBOSE("seems like connection has been destroyed, exiting");
            return;
        }

        if (m_httpClient.state() == nx::network::http::AsyncClient::State::sFailed
            || !m_httpClient.response())
        {
            LOG_INFO("http client failed to connect to host");
            stopInAioThread();
            return;
        }

        const int statusCode = m_httpClient.response()->statusLine.statusCode;
        LOG_VERBOSE("http client status code: %d", statusCode);

        if (!nx::network::http::StatusCode::isSuccessCode(statusCode))
        {
            LOG_INFO("http client got invalid response code %d", statusCode);
            stopInAioThread();
            return;
        }

        LOG_VERBOSE("http client connected successfully to the %s",
            config.url.constData());

        auto validationError = websocket::validateResponse(m_httpClient.request(),
            *m_httpClient.response());
        if (validationError != websocket::Error::noError)
        {
            LOG_INFO("websocket handshake validation error: %d", validationError);
            stopInAioThread();
            return;
        }

        LOG_VERBOSE("websocket handshake response validated successfully");
        LOG_INFO("successfully connected, starting reading");

        m_websocket.reset(new websocket::WebSocket(m_httpClient.takeSocket(),
            websocket::FrameType::text));
        m_websocket->bindToAioThread(m_aioThread);

        m_httpClient.pleaseStopSync();

        m_websocket->start();
        m_websocket->readSomeAsync(&m_readBuffer,
            [self = shared_from_this(), this](SystemError::ErrorCode errorCode, size_t bytesRead)
            { onRead(errorCode, bytesRead); });
    }

    void onRead(SystemError::ErrorCode errorCode, size_t bytesRead)
    {
        if (m_stopped)
        {
            LOG_VERBOSE("seems like connection has been destroyed, exiting");
            return;
        }

        if (errorCode != SystemError::noError)
        {
            LOG_INFO("read failed with error code: %d", errorCode);
            stopInAioThread();
            return;
        }

        if (bytesRead == 0)
        {
            LOG_INFO("connection has been closed by remote peer");
            stopInAioThread();
            return;
        }

        LOG_INFO("got message");
        LOG_VERBOSE("message content: %s", m_readBuffer.constData());
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
        LOG_VERBOSE("stopping all connections");

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

static const char* const kProtocolNameOption = "--protocol-name";
static const char* const kUrlOption = "--url";
static const char* const kUserNameOption = "--username";
static const char* const kPasswordOption = "--password";
static const char* const kServerAddressOption = "--server-address";
static const char* const kServerPortOption = "--server-port";
static const char* const kRoleOption = "--role";
static const char* const kLogLevelOption = "--log-level";
static const char* const kHelpOption = "--help";

static const char* const kInvalidOptionsMessage = "invalid options, run with --help";

static void printHelp()
{
    printf("Usage:\n websocket-utility --url <url> --username <username> --password <password>\n");
    printf("OR\n ");
    printf("websocket-utility [--server-address <server-address> (default: 0.0.0.0)] " \
           "--server-port <server-port>\n");
    printf("Additional options:\n --log-level ['verbose', 'info'(default)]\n "\
           "--role ['client'(default) 'server']\n " \
           "--protocol-name <protocol-name>(default: 'nxp2p')\n --help\n");
}

static void prepareConfig(int argc, const char* argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], kHelpOption) == 0)
        {
            printHelp();
            exit(EXIT_SUCCESS);
        }

        if (argv[i][0] != '-' || i == argc - 1)
        {
            LOG_INFO("%s", kInvalidOptionsMessage);
            exit(EXIT_SUCCESS);
        }

        if (strcmp(argv[i], kProtocolNameOption) == 0)
            config.protocolName = argv[++i];
        else if (strcmp(argv[i], kUrlOption) == 0)
            config.url = argv[++i];
        else if (strcmp(argv[i], kUserNameOption) == 0)
            config.userName = argv[++i];
        else if (strcmp(argv[i], kPasswordOption) == 0)
            config.userPassword = argv[++i];
        else if (strcmp(argv[i], kServerAddressOption) == 0)
            config.serverAddress = argv[++i];
        else if (strcmp(argv[i], kServerPortOption) == 0)
            config.serverPort = strtol(argv[++i], nullptr, 10);
        else if (strcmp(argv[i], kRoleOption) == 0)
        {
            if (strcmp(argv[i + 1], "client") == 0)
                config.role = Role::client;
            if (strcmp(argv[i + 1], "server") == 0)
                config.role = Role::server;

            ++i;
        }
        else if (strcmp(argv[i], kLogLevelOption) == 0)
        {
            if (strcmp(argv[i + 1], "verbose") == 0)
                config.logLevel = LogLevel::verbose;
            if (strcmp(argv[i + 1], "info") == 0)
                config.logLevel = LogLevel::info;

            ++i;
        }
    }
}

static bool validateConfig()
{
    switch (config.role)
    {
        case Role::client:
            if (config.url.isEmpty() || config.userName.isEmpty() || config.userPassword.isEmpty())
            {
                LOG_INFO("invalid options, run with --help");
                return false;
            }
        return true;
    case Role::server:
        if (config.serverAddress.isEmpty() || config.serverPort <= 0
            || config.protocolName.isEmpty())
        {
            LOG_INFO("invalid options, run with --help");
            return false;
        }
        return true;
    default:
        return false;
    }

    return true;
}

#if defined(_WIN32)

static bool sigHandler(DWORD fdwCtrlType)
{
    LOG_INFO("received SIGINT, shutting down connections..");
    std::thread([]() { websocketConnectionsPoolInstance->stopAll(); }).detach();
    return true;
}

#elif defined(__linux__)

static void sigHandler(int /*signo*/)
{
    LOG_INFO("received SIGINT, shutting down connections..");
    std::thread([]() { websocketConnectionsPoolInstance->stopAll(); }).detach();
}

#endif // _WIN32

int main(int argc, const char *argv[])
{
    prepareConfig(argc, argv);
    if (!validateConfig())
        return -1;

    setvbuf(stdout, nullptr, _IONBF, 0);

    WebsocketConnectionsPool websocketConnectionsPool;
    websocketConnectionsPoolInstance = &websocketConnectionsPool;

    #if defined(_WIN32)
        if (!SetConsoleCtrlHandler(sigHandler, true))
        {
            LOG_INFO("failed to install Ctrl-C handler");
            return -1;
        }
    #elif defined(__linux__)
        if (signal(SIGINT, sigHandler) == SIG_ERR)
        {
            LOG_INFO("failed to install Ctrl-C handler");
            return -1;
        }
    #endif

    auto sgGuard = std::make_unique<SocketGlobalsHolder>(0);
    aio::Timer timer;
    config.aioThread = timer.getAioThread();

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
    LOG_VERBOSE("Exiting...");

    return 0;
}
