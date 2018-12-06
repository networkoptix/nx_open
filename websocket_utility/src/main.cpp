#include <functional>
#include <memory>
#include <mutex>
#include <stdio.h>
#include <thread>
#include <string.h>
#include <stdlib.h>

#include <signal.h>

#include <nx/network/aio/timer.h>
#include <nx/network/p2p_transport/p2p_http_client_transport.h>
#include <nx/network/p2p_transport/p2p_http_server_transport.h>
#include <nx/network/p2p_transport/p2p_websocket_transport.h>
#include <nx/network/websocket/websocket_handshake.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/utils/std/future.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/log/log_main.h>

using namespace nx::network;

enum class Role
{
    server,
    client
};

static const nx::Buffer kHandlerPath = "/testP2PConnection";
static const nx::Buffer kTestMessage = "Hello! Your connection seems to work";
static const nx::utils::log::Tag kWebsocketUtilityTag{QString("P2PUtility")};

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
} config;

class Waitable
{
public:
    Waitable(aio::AbstractAioThread* aioThread): m_aioThread(aioThread) {}

    void stop()
    {
        m_aioThread->post(nullptr, [this]() { stopInAioThread(); });
    }

    void waitForDone()
    {
        m_readyFuture.wait();
    }

protected:
    aio::AbstractAioThread* m_aioThread = nullptr;
    bool m_stopped = false;

    void stopInAioThread()
    {
        if (m_stopped)
            return;

        NX_VERBOSE(this, "connection: stop requested, shutting down");
        m_stopped = true;
        m_readyPromise.set_value();
    }

private:
    nx::utils::promise<void> m_readyPromise;
    nx::utils::future<void> m_readyFuture = m_readyPromise.get_future();
};

using WaitablePtr = std::shared_ptr<Waitable>;

class WaitablePool
{
public:
    void addWaitable(const WaitablePtr waitable)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopped)
        {
            waitable->stop();
            return;
        }

        m_waitables.insert(waitable);
    }

    void stopAll()
    {
        NX_VERBOSE(this, "stopping all waitables");

        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopped)
            return;

        m_stopped = true;
        for (const auto& waitable: m_waitables)
            waitable->stop();
    }

    void waitAll()
    {
        for (const auto& waitable: m_waitables)
            waitable->waitForDone();
    }

private:
    std::mutex m_mutex;
    bool m_stopped = false;
    std::set<WaitablePtr> m_waitables;
};

static WaitablePool* waitablePoolInstance;

class P2PConnection;
using P2PConnectionPtr = std::shared_ptr<P2PConnection>;

class P2PConnection:
    public Waitable,
    public std::enable_shared_from_this<P2PConnection>
{
public:
    P2PConnection(aio::AbstractAioThread* aioThread): Waitable(aioThread)
    {
        m_httpClient->bindToAioThread(aioThread);
    }

    P2PConnection(
        aio::AbstractAioThread *aioThread,
        P2pTransportPtr p2pTransport): Waitable(aioThread)
    {
        m_p2pTransport = std::move(p2pTransport);
        m_timer.bindToAioThread(aioThread);
        m_p2pTransport->bindToAioThread(aioThread);
    }

    void connectAsync(nx::utils::MoveOnlyFunc<void()> completionHandler)
    {
        http::HttpHeaders websocketHeaders;
        websocket::addClientHeaders(&websocketHeaders, config.protocolName);

        NX_VERBOSE(this, lm("connecting to %1").args(config.url));

        m_httpClient->setUserName(config.userName);
        m_httpClient->setUserPassword(config.userPassword);
        m_httpClient->addRequestHeaders(websocketHeaders);
        m_httpClient->doGet(
            config.url,
            [self = shared_from_this(),
            this, completionHandler = std::move(completionHandler)]() mutable
            {
                onConnect(std::move(completionHandler));
            });
    }

    void startReading()
    {
        m_p2pTransport->readSomeAsync(
            &m_readBuffer,
            [self = shared_from_this(), this](SystemError::ErrorCode errorCode, size_t bytesRead)
            { onRead(errorCode, bytesRead); });
    }

    void startSending()
    {
        NX_VERBOSE(this, "Starting sending test messages to the peer");
        m_p2pTransport->sendAsync(
            kTestMessage,
            [self = shared_from_this(), this] (SystemError::ErrorCode errorCode,
                size_t bytesRead)
            {
                onSend(errorCode, bytesRead);
            });
    }

private:
    std::unique_ptr<http::AsyncClient> m_httpClient = std::make_unique<http::AsyncClient>();
    P2pTransportPtr m_p2pTransport;
    nx::Buffer m_readBuffer;
    aio::Timer m_timer;

    void onConnect(nx::utils::MoveOnlyFunc<void()> completionHandler)
    {
        if (m_stopped)
        {
            NX_VERBOSE(this, "seems like connection has been destroyed, exiting");
            return;
        }

        if (m_httpClient->state() == nx::network::http::AsyncClient::State::sFailed
            || !m_httpClient->response())
        {
            NX_INFO(this, "http client failed to connect to host");
            stopInAioThread();
            return;
        }

        const int statusCode = m_httpClient->response()->statusLine.statusCode;
        NX_VERBOSE(this, lm("http client status code: %1").args(statusCode));
        NX_VERBOSE(this, lm("http client connected successfully to the %1").args(config.url));

        if (statusCode == http::StatusCode::switchingProtocols)
        {
            NX_INFO(this, "Server reported readiness for a websocket connection");

            auto validationError = websocket::validateResponse(
                m_httpClient->request(),
                *m_httpClient->response());

            if (validationError != websocket::Error::noError)
            {
                NX_INFO(
                    this,
                    lm("websocket handshake validation error: %1").args((int) validationError));
                stopInAioThread();
                return;
            }

            NX_VERBOSE(this, "websocket handshake response validated successfully");
            NX_INFO(this, "successfully connected via WEBSOCKET, starting reading");

            m_p2pTransport.reset(new P2PWebsocketTransport(
                m_httpClient->takeSocket(),
                websocket::FrameType::text));

            m_p2pTransport->bindToAioThread(m_aioThread);
            m_httpClient->pleaseStopSync();
        }
        else if (statusCode == http::StatusCode::ok)
        {
            NX_INFO(this, "Server reported readiness for a http connection");

            m_p2pTransport.reset(new P2PHttpClientTransport(
                std::move(m_httpClient),
                websocket::FrameType::text));
        }
        else
        {
            NX_INFO(this, lm("http client got invalid response code %1").args(statusCode));
            stopInAioThread();
            return;
        }

        completionHandler();
    }

    void onRead(SystemError::ErrorCode errorCode, size_t bytesRead)
    {
        if (m_stopped)
        {
            NX_VERBOSE(this, "seems like connection has been destroyed, exiting");
            return;
        }

        if (errorCode != SystemError::noError)
        {
            NX_INFO(this, lm("read failed with error code: %1").args(errorCode));
            stopInAioThread();
            return;
        }

        if (bytesRead == 0)
        {
            NX_INFO(this, "connection has been closed by remote peer");
            stopInAioThread();
            return;
        }

        NX_INFO(this, "got message");
        NX_VERBOSE(this, lm("message content: %1").args(m_readBuffer));
        m_readBuffer = nx::Buffer();

        m_p2pTransport->readSomeAsync(&m_readBuffer,
            [self = shared_from_this(), this](SystemError::ErrorCode errorCode, size_t bytesRead)
            { onRead(errorCode, bytesRead); });
    }

    void onSend(SystemError::ErrorCode errorCode, size_t bytesRead)
    {
        if (m_stopped)
        {
            NX_VERBOSE(this, "seems like connection has been destroyed, exiting");
            return;
        }
        if (errorCode != SystemError::noError)
        {
            NX_INFO(this, lm("send failed with error code: %1").args(errorCode));
            stopInAioThread();
            return;
        }
        NX_INFO(this, "message sent");
        m_timer.start(std::chrono::seconds(1),
            [self = shared_from_this(), this]()
            {
                if (m_stopped)
                {
                    NX_VERBOSE(this, "seems like connection has been destroyed, exiting");
                    return;
                }
                m_p2pTransport->sendAsync(
                    kTestMessage,
                    [self = shared_from_this(), this] (SystemError::ErrorCode errorCode,
                        size_t bytesRead)
                    {
                        onSend(errorCode, bytesRead);
                    });
            });
    }
};

class P2PConnectionAcceptor: public Waitable
{
public:
    P2PConnectionAcceptor(
        aio::AbstractAioThread* aioThread,
        std::function<void(P2PConnectionPtr)> userCompletionHandler)
        :
        Waitable(aioThread),
        m_userCompletionHandler(userCompletionHandler)
    {
        m_httpServer.bindToAioThread(aioThread);
        m_httpServer.registerRequestProcessorFunc(
            kHandlerPath,
            [this](
                http::RequestContext requestContext,
                http::RequestProcessedHandler requestCompletionHandler)
            {
                onAccept(std::move(requestContext), std::move(requestCompletionHandler));
            }, http::Method::get);
    }

    bool startListening()
    {
        SocketAddress serverSocketAddress(
            QString::fromLatin1(config.serverAddress),
            config.serverPort);

        if (!m_httpServer.bindAndListen(serverSocketAddress))
        {
            NX_INFO(
                this,
                lm("failed to start http server, address: %1, port: %2")
                    .args(config.serverAddress, config.serverPort));

            return false;
        }

        NX_INFO(this, lm("starting to accept websocket connections via %1").args(kHandlerPath));
        return true;
    }

private:
    http::TestHttpServer m_httpServer;
    std::function<void(P2PConnectionPtr)> m_userCompletionHandler;

    void onAccept(
        http::RequestContext requestContext,
        http::RequestProcessedHandler requestCompletionHandler)
    {
        if (m_stopped)
        {
            NX_VERBOSE(this, "accepted request while in stopped state, exiting");
            return;
        }

        auto error = websocket::validateRequest(requestContext.request, requestContext.response);
        if (error != websocket::Error::noError)
        {
            NX_INFO(this, lm("error validating websocket request: %1").args((int) error));
            NX_INFO(this, lm("switching to http mode"));

            requestContext.connection->setSendCompletionHandler(
                [connection = requestContext.connection, this](SystemError::ErrorCode ecode)
                {
                    auto p2pTransport = P2pTransportPtr(
                        new P2PHttpServerTransport(
                            connection->takeSocket(),
                            nx::network::websocket::FrameType::text));

                    auto p2pConnection = std::make_shared<P2PConnection>(
                        config.aioThread,
                        std::move(p2pTransport));

                    m_userCompletionHandler(p2pConnection);
                });

            requestCompletionHandler(http::StatusCode::ok);
        }
        else
        {
            NX_VERBOSE(
                this,
                "received websocket connection request, headers validated succesfully");

            requestContext.connection->setSendCompletionHandler(
                [connection = requestContext.connection, this](SystemError::ErrorCode ecode)
                {
                    auto p2pTransport = P2pTransportPtr(
                        new P2PWebsocketTransport(
                            connection->takeSocket(),
                            nx::network::websocket::FrameType::text));

                    auto p2pConnection = std::make_shared<P2PConnection>(
                        config.aioThread,
                        std::move(p2pTransport));

                    m_userCompletionHandler(p2pConnection);
                });

            requestCompletionHandler(nx::network::http::StatusCode::switchingProtocols);
        }
    }
};

static void connectAndListen()
{
    auto p2pConnection = std::make_shared<P2PConnection>(config.aioThread);
    waitablePoolInstance->addWaitable(p2pConnection);
    p2pConnection->connectAsync([p2pConnection]() { p2pConnection->startReading(); });
}

static void startAccepting()
{
    auto acceptor = std::make_shared<P2PConnectionAcceptor>(
        config.aioThread,
        [](P2PConnectionPtr P2PConnection)
        {
            if (!P2PConnection)
                return;
            waitablePoolInstance->addWaitable(P2PConnection);
            P2PConnection->startSending();
        });

    if (!acceptor->startListening())
        return;

    waitablePoolInstance->addWaitable(acceptor);
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
    printf("Usage:\n CLIENT MODE: p2p_utility --url <url> --username <username> " \
           "--password <password>\n");
    printf(" SERVER MODE: p2p_utility [--server-address <server-address> (default: 0.0.0.0)] " \
           "--server-port <server-port>\n");
    printf("Additional options:\n --log-level <'verbose' OR 'info'(default)>\n "\
           "--role <'client'(default) OR 'server'>\n " \
           "--protocol-name <protocol-name(default: 'nxp2p')>\n --help\n");
    printf("Note:\n");
    printf(" In case if you want to connect to the p2p utility run in a server mode, client"
           " url path must be '/testP2PConnection'.\n");
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
            NX_INFO(kWebsocketUtilityTag, kInvalidOptionsMessage);
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
            {
                nx::utils::log::mainLogger()->setLevelFilters(nx::utils::log::LevelFilters{
                    {QnLog::MAIN_LOG_ID, nx::utils::log::Level::verbose}});
            }
            else if (strcmp(argv[i + 1], "info") == 0)
            {
                nx::utils::log::mainLogger()->setLevelFilters(nx::utils::log::LevelFilters{
                    {QnLog::MAIN_LOG_ID, nx::utils::log::Level::info}});
            }
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
                NX_INFO(kWebsocketUtilityTag, "invalid options, run with --help");
                return false;
            }
        return true;
    case Role::server:
        if (config.serverAddress.isEmpty() || config.serverPort <= 0
            || config.protocolName.isEmpty())
        {
            NX_INFO(kWebsocketUtilityTag, "invalid options, run with --help");
            return false;
        }
        return true;
    default:
        return false;
    }

    return true;
}

#if defined(_WIN32)

static BOOL sigHandler(DWORD fdwCtrlType)
{
    NX_INFO(kWebsocketUtilityTag, "received SIGINT, shutting down connections..");
    std::thread([]() { waitablePoolInstance->stopAll(); }).detach();
    return true;
}

#elif defined(__linux__)

static void sigHandler(int /*signo*/)
{
    NX_INFO(kWebsocketUtilityTag, "received SIGINT, shutting down connections..");
    std::thread([]() { waitablePoolInstance->stopAll(); }).detach();
}

#endif // _WIN32

int main(int argc, const char *argv[])
{
    prepareConfig(argc, argv);
    if (!validateConfig())
        return -1;

    auto sgGuard = std::make_unique<SocketGlobalsHolder>(0);
    WaitablePool waitablePool;
    waitablePoolInstance = &waitablePool;

    #if defined(_WIN32)
        if (!SetConsoleCtrlHandler(sigHandler, true))
        {
            NX_INFO(kWebsocketUtilityTag, "failed to install Ctrl-C handler");
            return -1;
        }
    #elif defined(__linux__)
        if (signal(SIGINT, sigHandler) == SIG_ERR)
        {
            NX_INFO(kWebsocketUtilityTag, "failed to install Ctrl-C handler");
            return -1;
        }
    #endif

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

    waitablePool.waitAll();
    NX_VERBOSE(kWebsocketUtilityTag, "Exiting...");

    return 0;
}
