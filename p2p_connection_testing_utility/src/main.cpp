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
#include <nx/network/http/custom_headers.h>
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
    automatic
};

static const nx::Buffer kHandlerPath = "/testP2P";
static const nx::Buffer kTestMessage = "Hello! Your connection seems to be working";

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
    Mode mode = Mode::automatic;
} config;

class P2PConnection;
using P2PConnectionPtr = std::shared_ptr<P2PConnection>;

class P2PConnection: public std::enable_shared_from_this<P2PConnection>
{
public:
    P2PConnection(aio::AbstractAioThread* aioThread): m_aioThread(aioThread)
    {
        m_httpClient->bindToAioThread(aioThread);
        m_timer.bindToAioThread(aioThread);
    }

    P2PConnection(
        aio::AbstractAioThread *aioThread,
        P2pTransportPtr p2pTransport): m_aioThread(aioThread)
    {
        m_httpClient->bindToAioThread(aioThread);
        m_timer.bindToAioThread(aioThread);
        m_p2pTransport = std::move(p2pTransport);
    }

    void connectAndStart()
    {
        m_guid = QnUuid::createUuid().toByteArray();

        http::HttpHeaders additionalHeaders;
        additionalHeaders.emplace(Qn::EC2_CONNECTION_GUID_HEADER_NAME, m_guid);
        websocket::addClientHeaders(&additionalHeaders, config.protocolName);

        NX_VERBOSE(
            this,
            lm("connectAsync: connecting to %1, connection GUID: %2").args(config.url, m_guid));

        m_httpClient->setUserName(config.userName);
        m_httpClient->setUserPassword(config.userPassword);
        m_httpClient->addRequestHeaders(additionalHeaders);
        m_httpClient->doGet(config.url, [self = shared_from_this(), this]() { onConnect(); });
    }

    void gotIncomingPostConnection(
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        const nx::Buffer& body)
    {
        auto p2pHttpServerConnection =
            dynamic_cast<nx::network::P2PHttpServerTransport*>(m_p2pTransport.get());

        p2pHttpServerConnection->gotPostConnection(std::move(socket), body);
    }

    void start(bool startTransport = true)
    {
        NX_VERBOSE(this, "start: Starting exchanging test messages with the peer");

        if (startTransport)
            m_p2pTransport->start();

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

    void onConnect()
    {
        if (m_httpClient->state() == nx::network::http::AsyncClient::State::sFailed
            || !m_httpClient->response())
        {
            NX_ERROR(this, "onConnect: Http client failed to connect to host");
            exit(EXIT_FAILURE);
        }

        const int statusCode = m_httpClient->response()->statusLine.statusCode;
        NX_VERBOSE(this, lm("onConnect: Http client status code: %1").args(statusCode));
        NX_VERBOSE(
            this,
            lm("onConnect: Http client connected successfully to the %1").args(config.url));

        if (statusCode == http::StatusCode::switchingProtocols)
        {
            NX_INFO(this, "onConnect: Server reported readiness for a websocket connection");

            auto validationError = websocket::validateResponse(
                m_httpClient->request(),
                *m_httpClient->response());

            if (validationError != websocket::Error::noError)
            {
                NX_ERROR(
                    this,
                    lm("onConnect: Websocket handshake validation error: %1")
                        .args((int) validationError));
                exit(EXIT_FAILURE);
            }

            NX_INFO(this, "onConnect: Successfully connected using WebSocket");

            m_p2pTransport.reset(new P2PWebsocketTransport(
                m_httpClient->takeSocket(),
                websocket::FrameType::text));

            m_p2pTransport->bindToAioThread(m_aioThread);
            m_httpClient->pleaseStopSync();
        }
        else if (statusCode == http::StatusCode::forbidden || statusCode == http::StatusCode::ok)
        {
            NX_INFO(
                this,
                "onConnect: Server reported websocket unavailability. Trying http mode.");

            m_p2pTransport.reset(new P2PHttpClientTransport(
                std::move(m_httpClient),
                m_guid,
                websocket::FrameType::text));

            m_p2pTransport->bindToAioThread(m_aioThread);
        }
        else
        {
            NX_ERROR(
                this,
                lm("onConnect: Server reported %1. Which is unexpected.").args(statusCode));
            exit(EXIT_FAILURE);
        }

        start();
    }

    void onRead(SystemError::ErrorCode errorCode, size_t bytesRead)
    {
        if (errorCode != SystemError::noError)
        {
            NX_ERROR(this, lm("onRead: Read failed with an error code: %1").args(errorCode));
            exit(EXIT_FAILURE);
        }

        if (bytesRead == 0)
        {
            NX_ERROR(this, "onRead: Connection has been closed by the remote peer");
            exit(EXIT_FAILURE);
        }

        NX_INFO(this, lm("onRead: got message: %1").args(m_readBuffer));
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
            NX_ERROR(this, lm("onSend: Send failed with an error code: %1").args(errorCode));
            exit(EXIT_FAILURE);
        }

        NX_INFO(this, "onSend: message sent");
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
        m_connections.insert(waitable);
    }

    P2PConnectionPtr findByGuid(const QByteArray& guid)
    {
        std::set<P2PConnectionPtr> connections;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            connections = m_connections;
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
    std::set<P2PConnectionPtr> m_connections;
};

static ConnectionPool* connectionPoolInstance;

class P2PConnectionAcceptor
{
public:
    P2PConnectionAcceptor(aio::AbstractAioThread* aioThread): m_aioThread(aioThread)
    {
        m_aioTimer.bindToAioThread(aioThread);
        m_httpServer.bindToAioThread(aioThread);

        m_httpServer.registerRequestProcessorFunc(
            kHandlerPath,
            [this](
                http::RequestContext requestContext,
                http::RequestProcessedHandler requestCompletionHandler)
            {
                onAcceptGET(std::move(requestContext), std::move(requestCompletionHandler));
            }, http::Method::get);

        m_httpServer.registerRequestProcessorFunc(
            kHandlerPath,
            [this](
                http::RequestContext requestContext,
                http::RequestProcessedHandler /*requestCompletionHandler*/)
            {
                onAcceptPOST(std::move(requestContext));
            }, http::Method::post);
    }

    bool startAccepting()
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

        NX_INFO(this, lm("starting to accept p2p connections via %1").args(kHandlerPath));
        return true;
    }

private:
    http::TestHttpServer m_httpServer;
    aio::AbstractAioThread* m_aioThread = nullptr;
    aio::Timer m_aioTimer;

    void onAcceptPOST(http::RequestContext requestContext)
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
                auto guidHeaderIt = sharedContext->headers.find(
                    Qn::EC2_CONNECTION_GUID_HEADER_NAME);

                if (guidHeaderIt == sharedContext->headers.cend())
                {
                    NX_ERROR(
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
                        NX_ERROR(
                            this,
                            "onPost: Failed to find the matching connection in the pool");
                        exit(EXIT_FAILURE);
                    }
                    else
                    {
                        m_aioTimer.start(
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

        m_aioTimer.start(
            std::chrono::milliseconds(100),
            [sharedContext, this]
            {
                sharedContext->checkForPairFunc();
            });

    }

    void onAcceptGET(
        http::RequestContext requestContext,
        http::RequestProcessedHandler requestCompletionHandler)
    {
        auto error = websocket::validateRequest(requestContext.request, requestContext.response);
        if (error != websocket::Error::noError || config.mode == Mode::http)
        {
            if (config.mode != Mode::http)
            {
                NX_INFO(
                    this,
                    lm("onAcceptGET: A websocket request validation error: %1").args((int) error));
            }

            NX_INFO(this, lm("onAcceptGET: Switching to the http mode"));

            requestContext.connection->setSendCompletionHandler(
                [connection = requestContext.connection,
                headers = requestContext.request.headers,
                this](SystemError::ErrorCode ecode)
                {
                    if (ecode != SystemError::noError)
                    {
                        NX_ERROR(
                            this,
                            "onAcceptGET: Failed to respond to the incoming connection request");

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
                                NX_ERROR(
                                    this,
                                    "P2PHttpServerTransport::onGetConnectionAccepted: Failed to "
                                    "start Http server connection");
                                exit(EXIT_FAILURE);
                            }

                            auto guidHeaderIt = headers.find(Qn::EC2_CONNECTION_GUID_HEADER_NAME);
                            if (guidHeaderIt == headers.cend())
                            {
                                NX_ERROR(
                                    this,
                                    "P2PHttpServerTransport::onGetConnectionAccepted: No GUID "
                                    "header in the incoming Http request headers");
                                exit(EXIT_FAILURE);
                            }

                            auto p2pConnection = std::make_shared<P2PConnection>(
                                m_aioThread,
                                P2pTransportPtr(p2pTransport));

                            p2pConnection->setGuid(guidHeaderIt->second);
                            connectionPoolInstance->addConnection(p2pConnection);

                            NX_INFO(
                                typeid(P2PConnectionAcceptor),
                                "P2PHttpServerTransport::onGetConnectionAccepted: Accepted "
                                "connection and added to the Pool");

                            p2pConnection->start(/*startTransport*/ false);
                        });
                });

            requestCompletionHandler(http::StatusCode::ok);
        }
        else
        {
            NX_VERBOSE(
                this,
                "onAcceptGET: Received websocket connection request, headers validated"
                " successfully");

            requestContext.connection->setSendCompletionHandler(
                [connection = requestContext.connection, this](SystemError::ErrorCode ecode)
                {
                    if (ecode != SystemError::noError)
                    {
                        NX_ERROR(
                            this,
                            "onAcceptGET: Failed to respond to the incoming connection");
                        exit(EXIT_FAILURE);
                    }

                    auto p2pTransport = P2pTransportPtr(
                        new P2PWebsocketTransport(
                            connection->takeSocket(),
                            nx::network::websocket::FrameType::text));

                    p2pTransport->bindToAioThread(m_aioThread);

                    auto p2pConnection = std::make_shared<P2PConnection>(
                        config.aioThread,
                        std::move(p2pTransport));

                    connectionPoolInstance->addConnection(p2pConnection);
                    p2pConnection->start();
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
           "[--server-address <server-address> (default: 0.0.0.0)" \
           "--force-http (Forces server to accept http p2p connection only)]\n");
    printf("Additional options:\n" \
           " --verbose\n" \
           " --help\n");
    printf("Note:\n");
    printf(" In case if you want to connect to the p2p_connection_testing_utility run in the "
           "server mode, the client url path must be '/testP2P'.\n");
}

static bool parseCommandLineArguments(int argc, const char* argv[])
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
            return false;

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
        else if (strcmp(argv[i], "--force-http") == 0)
        {
            config.mode = Mode::http;
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

    return true;
}

static bool validateConfig()
{
    if (config.role == Role::client && config.url.isEmpty())
        return false;

    if (config.role == Role::server && (config.serverAddress.isEmpty() || config.serverPort <= 0))
        return false;

    return true;
}

int main(int argc, const char *argv[])
{
    if (!parseCommandLineArguments(argc, argv) || !validateConfig())
    {
        NX_INFO(nx::utils::log::Tag(QString("P2PUtility")), "invalid options");
        printHelp();
        exit(EXIT_FAILURE);
    }

    auto sgGuard = std::make_unique<SocketGlobalsHolder>(0);
    ConnectionPool connectionPool;
    connectionPoolInstance = &connectionPool;

    aio::Timer timer;
    config.aioThread = timer.getAioThread();

    P2PConnectionPtr p2pConnection;
    P2PConnectionAcceptorPtr p2pConnectionAcceptor;

    switch (config.role)
    {
        case Role::server:
            p2pConnectionAcceptor = std::make_shared<P2PConnectionAcceptor>(config.aioThread);

            if (!p2pConnectionAcceptor->startAccepting())
            {
                NX_ERROR(
                    typeid(P2PConnectionAcceptor),
                    "Failed to start P2P connection acceptor");

                exit(EXIT_FAILURE);
            }

            break;

        case Role::client:
            p2pConnection = std::make_shared<P2PConnection>(config.aioThread);
            p2pConnection->connectAndStart();
            break;

        default:
            NX_ASSERT(false);
            exit(EXIT_FAILURE);
    }

    while (1)
        std::this_thread::sleep_for(std::chrono::minutes(1));

    return 0;
}
