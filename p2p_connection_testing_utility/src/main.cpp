#include <functional>
#include <memory>
#include <mutex>
#include <stdio.h>
#include <thread>
#include <string.h>
#include <algorithm>
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

enum class Mode
{
    http,
    websocket,
    automatic
};

static const nx::Buffer kHandlerPath = "/testP2P";
static const nx::Buffer kTestMessage = "Hello! Your connection seems to be working";
static const nx::utils::log::Tag kP2PConnectionTestingUtilityTag{QString("P2PUtility")};

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
    aio::Timer* aioTimer = nullptr;
    Mode mode = Mode::automatic;
} config;

class P2PConnection;
using P2PConnectionPtr = std::shared_ptr<P2PConnection>;

class P2PConnection:
    public std::enable_shared_from_this<P2PConnection>
{
public:
    P2PConnection(aio::AbstractAioThread* aioThread): m_aioThread(aioThread)
    {
        m_httpClient->bindToAioThread(aioThread);
    }

    P2PConnection(
        aio::AbstractAioThread *aioThread,
        P2pTransportPtr p2pTransport): m_aioThread(aioThread)
    {
        m_p2pTransport = std::move(p2pTransport);
        m_timer.bindToAioThread(aioThread);
    }

    void connectAsync(nx::utils::MoveOnlyFunc<void()> completionHandler)
    {
        const auto connectionGuid = QnUuid::createUuid().toByteArray();

        http::HttpHeaders additionalHeaders;
        additionalHeaders.emplace("X-NX-P2P-GUID",connectionGuid);
        websocket::addClientHeaders(&additionalHeaders, config.protocolName);

        NX_VERBOSE(this, lm("connecting to %1").args(config.url));

        m_httpClient->setUserName(config.userName);
        m_httpClient->setUserPassword(config.userPassword);
        m_httpClient->addRequestHeaders(additionalHeaders);
        m_httpClient->doGet(
            config.url,
            [self = shared_from_this(),
            this,
            completionHandler = std::move(completionHandler),
            connectionGuid]() mutable
            {
                onConnect(std::move(completionHandler), connectionGuid);
            });
    }

    void gotIncomingPostConnection(
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        const nx::Buffer& body)
    {
        auto p2pHttpServerConnection =
            dynamic_cast<nx::network::P2PHttpServerTransport*>(m_p2pTransport.get());

        p2pHttpServerConnection->gotPostConnection(std::move(socket), body);
    }

    void start()
    {
        NX_VERBOSE(this, "Starting sending test messages to the peer");
        m_p2pTransport->sendAsync(
            kTestMessage,
            [self = shared_from_this(), this] (SystemError::ErrorCode errorCode,
                size_t bytesRead)
            {
                onSend(errorCode, bytesRead);
            });

        m_p2pTransport->readSomeAsync(
            &m_readBuffer,
            [self = shared_from_this(), this](SystemError::ErrorCode errorCode, size_t bytesRead)
            {
                onRead(errorCode, bytesRead);
            });
    }

    void setGuid(const QByteArray& guid) { m_guid = guid; }
    QByteArray guid() const { return m_guid; }

private:
    std::unique_ptr<http::AsyncClient> m_httpClient = std::make_unique<http::AsyncClient>();
    P2pTransportPtr m_p2pTransport;
    nx::Buffer m_readBuffer;
    aio::Timer m_timer;
    QByteArray m_guid;
    aio::AbstractAioThread* m_aioThread = nullptr;

    void onConnect(
        nx::utils::MoveOnlyFunc<void()> completionHandler,
        const QByteArray& connectionGuid)
    {
        if (m_httpClient->state() == nx::network::http::AsyncClient::State::sFailed
            || !m_httpClient->response())
        {
            NX_INFO(this, "http client failed to connect to host");
            exit(EXIT_FAILURE);
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
                exit(EXIT_FAILURE);
            }

            NX_VERBOSE(this, "websocket handshake response validated successfully");
            NX_INFO(this, "successfully connected via WEBSOCKET, starting reading");

            m_p2pTransport.reset(new P2PWebsocketTransport(
                m_httpClient->takeSocket(),
                websocket::FrameType::text));

            m_p2pTransport->bindToAioThread(m_aioThread);
            m_p2pTransport->start();
            m_httpClient->pleaseStopSync();
        }
        else if (statusCode == http::StatusCode::forbidden || statusCode == http::StatusCode::ok)
        {
            NX_INFO(this, "Server reported forbidden. Trying http mode.");

            m_p2pTransport.reset(new P2PHttpClientTransport(
                std::move(m_httpClient),
                connectionGuid,
                websocket::FrameType::text));
            m_p2pTransport->bindToAioThread(m_aioThread);
            m_p2pTransport->start();
        }
        else
        {
            NX_INFO(this, lm("Server reported %1. Which is unexpected.").args(statusCode));
            exit(EXIT_FAILURE);
        }

        completionHandler();
    }

    void onRead(SystemError::ErrorCode errorCode, size_t bytesRead)
    {
        if (errorCode != SystemError::noError)
        {
            NX_INFO(this, lm("read failed with error code: %1").args(errorCode));
            exit(EXIT_FAILURE);
        }

        if (bytesRead == 0)
        {
            NX_INFO(this, "connection has been closed by remote peer");
            exit(EXIT_FAILURE);
        }

        NX_INFO(this, "got message");
        NX_VERBOSE(this, lm("message content: %1").args(m_readBuffer));
        m_readBuffer = nx::Buffer();

        m_p2pTransport->readSomeAsync(
            &m_readBuffer,
            [self = shared_from_this(), this](SystemError::ErrorCode errorCode, size_t bytesRead)
            { onRead(errorCode, bytesRead); });
    }

    void onSend(SystemError::ErrorCode errorCode, size_t /*bytesSent*/)
    {
        if (errorCode != SystemError::noError)
        {
            NX_INFO(this, lm("send failed with error code: %1").args(errorCode));
            exit(EXIT_FAILURE);
        }

        NX_INFO(this, "message sent");
        m_timer.start(
            std::chrono::seconds(1),
            [self = shared_from_this(), this]()
            {
                m_p2pTransport->sendAsync(
                    kTestMessage,
                    [self = shared_from_this(), this] (SystemError::ErrorCode errorCode,
                        size_t bytesSent)
                    {
                        onSend(errorCode, bytesSent);
                    });
            });
    }
};

class ConnectionPool
{
public:
    void addConnection(const P2PConnectionPtr waitable)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_waitables.insert(waitable);
    }

    P2PConnectionPtr findByGuid(const QByteArray& guid)
    {
        std::set<P2PConnectionPtr> connections;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            connections = m_waitables;
        }

        for (const auto& connection : connections)
        {
            if (connection->guid() == guid)
                return connection;
        }

        return P2PConnectionPtr();
    }

private:
    std::mutex m_mutex;
    bool m_stopped = false;
    std::set<P2PConnectionPtr> m_waitables;
};

static ConnectionPool* connectionPoolInstance;

class P2PConnectionAcceptor
{
public:
    P2PConnectionAcceptor(aio::AbstractAioThread* aioThread): m_aioThread(aioThread)
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

        m_httpServer.registerRequestProcessorFunc(
            kHandlerPath,
            [this](
                http::RequestContext requestContext,
                http::RequestProcessedHandler /*requestCompletionHandler*/)
            {
                struct CheckForPairConnectionContext
                {
                    std::unique_ptr<AbstractStreamSocket> socket;
                    http::HttpHeaders headers;
                    int iterations = 0;
                    nx::utils::MoveOnlyFunc<void()> checkForPairFunc = nullptr;
                    nx::Buffer body;
                };

                auto sharedContext = std::make_shared<CheckForPairConnectionContext>();
                sharedContext->socket = requestContext.connection->takeSocket();
                sharedContext->headers = requestContext.request.headers;
                sharedContext->body = requestContext.request.messageBody;

                auto checkForPairConnection =
                    [sharedContext, this]() mutable
                    {
                        auto guidHeaderIt = sharedContext->headers.find("X-NX-P2P-GUID");
                        if (guidHeaderIt == sharedContext->headers.cend())
                        {
                            NX_INFO(
                                this,
                                "onPost: No GUID header in the incoming Http request headers");
                            exit(EXIT_FAILURE);
                        }

                        auto existingP2PConnection =
                            connectionPoolInstance->findByGuid(guidHeaderIt->second);
                        if (!existingP2PConnection)
                        {
                            if (++sharedContext->iterations > 30)
                            {
                                NX_INFO(
                                    this,
                                    "onPost: Failed to find the matching connection in the pool");
                                exit(EXIT_FAILURE);
                            }
                            else
                            {
                                config.aioTimer->start(
                                    std::chrono::milliseconds(100),
                                    [sharedContext, this]
                                    {
                                        sharedContext->checkForPairFunc();
                                    });
                                return;
                            }
                        }

                        NX_INFO(this, "onPost: Found pair connection in the pool");

                        auto p2pConnection =
                            std::dynamic_pointer_cast<P2PConnection>(existingP2PConnection);

                        p2pConnection->gotIncomingPostConnection(
                            std::move(sharedContext->socket),
                            sharedContext->body);
                    };

                sharedContext->checkForPairFunc = std::move(checkForPairConnection);

                config.aioTimer->start(
                    std::chrono::milliseconds(100),
                    [sharedContext, this]
                    {
                        sharedContext->checkForPairFunc();
                    });

            }, http::Method::post);
    }

    bool startAccepting(std::function<void(P2PConnectionPtr)> userCompletionHandler)
    {
        m_userCompletionHandler = userCompletionHandler;

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

        NX_INFO(this, lm("starting to accept p2p connections via %1").args(kHandlerPath));
        return true;
    }

private:
    http::TestHttpServer m_httpServer;
    std::function<void(P2PConnectionPtr)> m_userCompletionHandler;
    aio::AbstractAioThread* m_aioThread = nullptr;

    void onAccept(
        http::RequestContext requestContext,
        http::RequestProcessedHandler requestCompletionHandler)
    {
        auto error = websocket::validateRequest(requestContext.request, requestContext.response);
        if (error != websocket::Error::noError || config.mode == Mode::http)
        {
            if (config.mode != Mode::http)
                NX_INFO(this, lm("error validating websocket request: %1").args((int) error));

            NX_INFO(this, lm("switching to http mode"));

            requestContext.connection->setSendCompletionHandler(
                [connection = requestContext.connection,
                headers = requestContext.request.headers,
                this](SystemError::ErrorCode ecode)
                {
                    if (ecode != SystemError::noError)
                    {
                        NX_INFO(
                            this,
                            "onAccept: Failed to respond to the incoming connection request");

                        exit(EXIT_FAILURE);
                    }

                    auto p2pTransport = new P2PHttpServerTransport(
                        connection->takeSocket(),
                        nx::network::websocket::FrameType::text);

                    p2pTransport->bindToAioThread(m_aioThread);
                    p2pTransport->start(
                        [p2pTransport, this, headers = headers](
                            SystemError::ErrorCode error)
                        {
                            if (error != SystemError::noError)
                            {
                                NX_INFO(this, "onAccept: Failed to start Http server connection");
                                exit(EXIT_FAILURE);
                            }

                            auto p2pConnection = std::make_shared<P2PConnection>(
                                config.aioThread,
                                P2pTransportPtr(p2pTransport));

                            auto guidHeaderIt = headers.find("X-NX-P2P-GUID");
                            if (guidHeaderIt == headers.cend())
                            {
                                NX_INFO(
                                    this,
                                    "onGet: No GUID header in the incoming Http request headers");
                                exit(EXIT_FAILURE);
                            }

                            p2pConnection->setGuid(guidHeaderIt->second);
                            m_userCompletionHandler(p2pConnection);
                        });
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
                    if (ecode != SystemError::noError)
                    {
                        NX_INFO(this, "Failed to respond to the incoming connection");
                        exit(EXIT_FAILURE);
                    }

                    auto p2pTransport = P2pTransportPtr(
                        new P2PWebsocketTransport(
                            connection->takeSocket(),
                            nx::network::websocket::FrameType::text));

                    p2pTransport->bindToAioThread(m_aioThread);
                    p2pTransport->start();

                    auto p2pConnection = std::make_shared<P2PConnection>(
                        config.aioThread,
                        std::move(p2pTransport));

                    m_userCompletionHandler(p2pConnection);
                });

            requestCompletionHandler(nx::network::http::StatusCode::switchingProtocols);
        }
    }
};

using P2PConnectionAcceptorPtr = std::shared_ptr<P2PConnectionAcceptor>;

static void printHelp()
{
    printf("Usage:\n CLIENT MODE: p2p_connection_testing_utility --role client --url <url> " \
        "[--username <username> --password <password>]\n");
    printf(" SERVER MODE: p2p_connection_testing_utility --role server " \
           "--server-port <server-port> " \
           "[--server-address <server-address> (default: 0.0.0.0)]\n");
    printf("Additional options:\n" \
           " --verbose\n" \
           " --mode <'auto'(default) OR 'http' OR 'websocket'>\n" \
           " --help\n");
    printf("Note:\n");
    printf(" In case if you want to connect to the p2p_connection_testing_utility run in the "
           "server mode, the client url path must be '/testP2P'.\n");
}

static void prepareConfig(int argc, const char* argv[])
{
    bool verbose = false;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--help") == 0)
        {
            printHelp();
            exit(EXIT_SUCCESS);
        }

        if (argv[i][0] != '-')
        {
            NX_INFO(kP2PConnectionTestingUtilityTag, "invalid options, run with --help");
            exit(EXIT_FAILURE);
        }

        if (strcmp(argv[i], "--url") == 0 && i + 1 < argc)
            config.url = argv[++i];
        else if (strcmp(argv[i], "--username") == 0 && i + 1 < argc)
            config.userName = argv[++i];
        else if (strcmp(argv[i], "--password") == 0 && i + 1 < argc)
            config.userPassword = argv[++i];
        else if (strcmp(argv[i], "--server-address") == 0 && i + 1 < argc)
            config.serverAddress = argv[++i];
        else if (strcmp(argv[i], "--server-port") == 0 && i + 1 < argc)
            config.serverPort = strtol(argv[++i], nullptr, 10);
        else if (strcmp(argv[i], "--role") == 0 && i + 1 < argc)
        {
            if (strcmp(argv[i + 1], "client") == 0)
                config.role = Role::client;
            else if (strcmp(argv[i + 1], "server") == 0)
                config.role = Role::server;

            ++i;
        }
        else if (strcmp(argv[i], "--verbose") == 0)
        {
            verbose = true;
        }
        else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc)
        {
            if (strcmp(argv[i + 1], "http") == 0)
                config.mode = Mode::http;
            else if (strcmp(argv[i + 1], "websocket") == 0)
                config.mode = Mode::websocket;

            ++i;
        }
    }

    if (verbose)
    {
        nx::utils::log::mainLogger()->setLevelFilters(
            nx::utils::log::LevelFilters{{QnLog::MAIN_LOG_ID, nx::utils::log::Level::verbose}});
    }
    else
    {
        nx::utils::log::mainLogger()->setLevelFilters(
            nx::utils::log::LevelFilters{{QnLog::MAIN_LOG_ID, nx::utils::log::Level::info}});
    }
}

static bool validateConfig()
{
    switch (config.role)
    {
        case Role::client:
            if (config.url.isEmpty())
            {
                NX_INFO(kP2PConnectionTestingUtilityTag, "invalid options, run with --help");
                return false;
            }
        return true;
    case Role::server:
        if (config.serverAddress.isEmpty() || config.serverPort <= 0
            || config.protocolName.isEmpty())
        {
            NX_INFO(kP2PConnectionTestingUtilityTag, "invalid options, run with --help");
            return false;
        }
        return true;
    default:
        return false;
    }

    return true;
}

int main(int argc, const char *argv[])
{
    prepareConfig(argc, argv);

    if (!validateConfig())
    {
        printHelp();
        exit(EXIT_FAILURE);
    }

    auto sgGuard = std::make_unique<SocketGlobalsHolder>(0);
    ConnectionPool connectionPool;
    connectionPoolInstance = &connectionPool;

    aio::Timer timer;
    config.aioThread = timer.getAioThread();
    config.aioTimer = &timer;

    P2PConnectionPtr p2pConnection;
    P2PConnectionAcceptorPtr p2pConnectionAcceptor;

    switch (config.role)
    {
        case Role::server:
            p2pConnectionAcceptor = std::make_shared<P2PConnectionAcceptor>(config.aioThread);

            if (!p2pConnectionAcceptor->startAccepting(
                    [](P2PConnectionPtr p2pConnection)
                    {
                        if (!p2pConnection)
                            return;

                        connectionPoolInstance->addConnection(p2pConnection);
                        NX_INFO(
                            typeid(P2PConnectionAcceptor),
                            "Accepted connection and added to the Pool");

                        p2pConnection->start();
                    }))
                {
                    NX_INFO(
                        typeid(P2PConnectionAcceptor),
                        "Failed to start P2P connection acceptor");

                    exit(EXIT_FAILURE);
                }

            break;

        case Role::client:
            p2pConnection = std::make_shared<P2PConnection>(config.aioThread);
            p2pConnection->connectAsync([p2pConnection]() { p2pConnection->start(); });
            break;

        default:
            NX_ASSERT(false);
            exit(EXIT_FAILURE);
    }

    while (1)
        std::this_thread::sleep_for(std::chrono::minutes(1));

    return 0;
}
