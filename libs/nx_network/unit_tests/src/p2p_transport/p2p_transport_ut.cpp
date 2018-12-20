#include <gtest/gtest.h>
#include <nx/network/p2p_transport/p2p_http_server_transport.h>
#include <nx/network/p2p_transport/p2p_http_client_transport.h>
#include <nx/network/socket_factory.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/utils/std/future.h>

namespace nx::network::test {

class P2PHttpTransport: public ::testing::Test
{
protected:
    P2PHttpTransport()
    {
        if (m_serverSendBuffer.isEmpty())
        {
            for (int i = 0; i < 10000; ++i)
            {
                m_serverSendBuffer.push_back("hello");
                m_clientSendBuffer.push_back("hello");
            }
        }

        m_acceptor = SocketFactory::createStreamServerSocket();
        m_acceptor->setNonBlockingMode(true);
        m_acceptor->setReuseAddrFlag(true);
        m_acceptor->setReusePortFlag(true);
        m_acceptor->bind(SocketAddress::anyPrivateAddress);
        m_acceptor->listen();
    }

    virtual void TearDown() override
    {
        m_acceptor->pleaseStopSync();
    }

    void givenConnectedP2PTransports()
    {
        std::unique_ptr<AbstractStreamSocket> clientSocket;
        std::unique_ptr<AbstractStreamSocket> serverSocket;
        createConnectedSockets(&clientSocket, &serverSocket);

        m_acceptor->acceptAsync(
            [this](
                SystemError::ErrorCode error,
                std::unique_ptr<AbstractStreamSocket> acceptedSocket)
            {
                ASSERT_EQ(SystemError::noError, error);
                m_server->gotPostConnection(std::move(acceptedSocket));
                m_serverPostConnectionPromise.set_value();
            });

        ASSERT_TRUE((bool) clientSocket);
        ASSERT_TRUE((bool) serverSocket);

        auto clientHttpClient = std::make_unique<http::AsyncClient>(std::move(clientSocket));

        m_server.reset(new P2PHttpServerTransport(std::move(serverSocket)));
        m_client.reset(new P2PHttpClientTransport(
            std::move(clientHttpClient),
            QnUuid::createUuid().toByteArray(),
            websocket::binary,
            utils::Url("http://" + m_acceptor->getLocalAddress().toString() + kPath)));

        utils::promise<void> startedPromise;
        auto startedFuture = startedPromise.get_future();

        m_server->start(
            [&](SystemError::ErrorCode error)
            {
                ASSERT_EQ(SystemError::noError, error);
                startedPromise.set_value();
            });

        startedFuture.wait();
    }

    void assertDataIsSentFromServerToClientViaGETChannel()
    {
        auto connectionContext = std::make_shared<ConnectionContext>();
        auto readyFuture = connectionContext->promise.get_future();

        m_server->sendAsync(
            m_serverSendBuffer,
            [this, connectionContext](
                SystemError::ErrorCode error,
                size_t transferred)
            {
                onServerSend(connectionContext, error, transferred);
            });

        m_client->readSomeAsync(
            &m_clientReadBuffer,
            [this, connectionContext](SystemError::ErrorCode error, size_t transferred)
            {
                onClientRead(connectionContext, error, transferred);
            });

        readyFuture.wait();
    }

    void assertDataIsSentFromClientToServerViaPOSTChannel()
    {
        auto connectionContext = std::make_shared<ConnectionContext>();
        auto readyFuture = connectionContext->promise.get_future();

        m_client->sendAsync(
            m_clientSendBuffer,
            [this, connectionContext](SystemError::ErrorCode error, size_t transferred)
            {
                onClientSend(connectionContext, error, transferred);
            });

        m_serverPostConnectionFuture.wait();
        m_acceptor->pleaseStopSync();
        m_server->readSomeAsync(
            &m_serverReadBuffer,
            [this, connectionContext](SystemError::ErrorCode error, size_t transferred)
            {
                onServerRead(connectionContext, error, transferred);
            });

        readyFuture.wait();
    }

private:
    struct ConnectionContext
    {
        int sendCount = 0;
        int readCount  = 0;
        nx::utils::promise<void> promise;
    };

    static const QString kPath;
    std::shared_ptr<P2PHttpServerTransport> m_server;
    std::shared_ptr<P2PHttpClientTransport> m_client;
    static nx::Buffer m_serverSendBuffer;
    nx::Buffer m_clientReadBuffer;
    static nx::Buffer m_clientSendBuffer;
    nx::Buffer m_serverReadBuffer;
    std::unique_ptr<AbstractStreamServerSocket> m_acceptor;
    utils::promise<void> m_serverPostConnectionPromise;
    utils::future<void> m_serverPostConnectionFuture = m_serverPostConnectionPromise.get_future();

    void createConnectedSockets(
        std::unique_ptr<AbstractStreamSocket>* clientSocket,
        std::unique_ptr<AbstractStreamSocket>* serverSocket)
    {
        *clientSocket = SocketFactory::createStreamSocket();
        (*clientSocket)->setNonBlockingMode(true);

        nx::utils::promise<void> connectPromise;
        auto connectFuture = connectPromise.get_future();
        m_acceptor->acceptAsync(
            [&](SystemError::ErrorCode /*ecode*/, std::unique_ptr<AbstractStreamSocket> acceptedSocket)
            {
                *serverSocket = std::move(acceptedSocket);
                (*serverSocket)->setNonBlockingMode(true);
                connectPromise.set_value();
            });

        (*clientSocket)->connect(m_acceptor->getLocalAddress(), std::chrono::milliseconds(0));
        connectFuture.wait();
    }

    void onServerSend(
        std::shared_ptr<ConnectionContext> connectionContext,
        SystemError::ErrorCode error,
        size_t transferred)
    {
        if (++connectionContext->sendCount == 1000)
            return;

        ASSERT_EQ(SystemError::noError, error);
        ASSERT_LE(m_serverSendBuffer.size(), transferred);

        m_server->sendAsync(
            m_serverSendBuffer,
            [this, connectionContext](SystemError::ErrorCode error, size_t transferred)
            {
                onServerSend(connectionContext, error, transferred);
            });
    }

    void onClientRead(
        std::shared_ptr<ConnectionContext> connectionContext,
        SystemError::ErrorCode error,
        size_t transferred)
    {
        ASSERT_EQ(error, SystemError::noError);
        ASSERT_EQ(m_serverSendBuffer.size(), transferred);
        ASSERT_EQ(m_clientReadBuffer, m_serverSendBuffer);

        if (++connectionContext->readCount == 1000)
        {
            connectionContext->promise.set_value();
            return;
        }

        m_clientReadBuffer.clear();
        m_client->readSomeAsync(
            &m_clientReadBuffer,
            [this, connectionContext](SystemError::ErrorCode error, size_t transferred)
            {
                onClientRead(connectionContext, error, transferred);
            });
    }

    void onClientSend(
        std::shared_ptr<ConnectionContext> connectionContext,
        SystemError::ErrorCode error,
        size_t transferred)
    {
        if (++connectionContext->sendCount == 1000)
            return;

        ASSERT_EQ(SystemError::noError, error);
        ASSERT_LE(m_clientSendBuffer.size(), transferred);

        m_client->sendAsync(
            m_clientSendBuffer,
            [this, connectionContext](SystemError::ErrorCode error, size_t transferred)
            {
                onClientSend(connectionContext, error, transferred);
            });
    }

    void onServerRead(
        std::shared_ptr<ConnectionContext> connectionContext,
        SystemError::ErrorCode error,
        size_t transferred)
    {
        ASSERT_EQ(error, SystemError::noError);
        ASSERT_LE(m_clientSendBuffer.size(), transferred);
        ASSERT_EQ(m_serverReadBuffer, m_clientSendBuffer);

        if (++connectionContext->readCount == 1000)
        {
            connectionContext->promise.set_value();
            return;
        }

        m_serverReadBuffer.clear();
        m_server->readSomeAsync(
            &m_serverReadBuffer,
            [this, connectionContext](SystemError::ErrorCode error, size_t transferred)
            {
                onServerRead(connectionContext, error, transferred);
            });
    }
};

nx::Buffer P2PHttpTransport::m_serverSendBuffer;
nx::Buffer P2PHttpTransport::m_clientSendBuffer;
const QString P2PHttpTransport::kPath = "/p2pTest";

TEST_F(P2PHttpTransport, ClientReads)
{
    givenConnectedP2PTransports();
    assertDataIsSentFromServerToClientViaGETChannel();
}

TEST_F(P2PHttpTransport, ClientSends)
{
    givenConnectedP2PTransports();
    assertDataIsSentFromClientToServerViaPOSTChannel();
}

} // namespace nx::network::test
