#include <gtest/gtest.h>
#include <nx/network/p2p_transport/p2p_http_server_transport.h>
#include <nx/network/p2p_transport/p2p_http_client_transport.h>
#include <nx/network/socket_factory.h>
#include <nx/utils/std/future.h>

namespace nx::network::test {

class ConnectedSocketsSupplier
{
public:
    ConnectedSocketsSupplier();
    ~ConnectedSocketsSupplier();

    bool connectSockets();
    std::unique_ptr<AbstractStreamSocket> clientSocket();
    std::unique_ptr<AbstractStreamSocket> serverSocket();
    std::string lastError();

private:
    std::unique_ptr<AbstractStreamServerSocket> m_acceptor;
    std::unique_ptr<AbstractStreamSocket> m_clientSocket;
    std::unique_ptr<AbstractStreamSocket> m_acceptedClientSocket;
    std::string m_lastError;
    nx::utils::promise<std::string> m_connectedPromise;
    nx::utils::future<std::string> m_connectedFuture;

    template<typename F>
    bool checkedCall(F f, const std::string& errorString);
    bool initAcceptor();
    bool initClientSocket();
    bool ok() const;
    void onAccept(
        SystemError::ErrorCode ecode,
        std::unique_ptr<AbstractStreamSocket> acceptedSocket);
    bool connect();
};

ConnectedSocketsSupplier::ConnectedSocketsSupplier():
    m_connectedFuture(m_connectedPromise.get_future())
{
    if(!initAcceptor())
        return;

    initClientSocket();
}

ConnectedSocketsSupplier::~ConnectedSocketsSupplier()
{
    if (m_acceptor)
        m_acceptor->pleaseStopSync();

    if (m_clientSocket)
        m_clientSocket->pleaseStopSync();

    if (m_acceptedClientSocket)
        m_acceptedClientSocket->pleaseStopSync();
}

bool ConnectedSocketsSupplier::initAcceptor()
{
    if (!checkedCall(
            [this]()
            {
                return (bool) (m_acceptor = std::unique_ptr<AbstractStreamServerSocket>(
                    SocketFactory::createStreamServerSocket()));
            },
            "SocketFactory::createStreamServerSocket"))
    {
        return false;
    }

    if (!checkedCall(
            [this]() { return m_acceptor->setNonBlockingMode(true); },
            "acceptor::setNonBlockingMode"))
    {
        return false;
    }

    if (!checkedCall(
            [this]() { return m_acceptor->bind(SocketAddress::anyPrivateAddress); },
            "acceptor::bind"))
    {
        return false;
    }

    return checkedCall(
            [this]() { return m_acceptor->listen(); },
            "acceptor::listen");
}

bool ConnectedSocketsSupplier::initClientSocket()
{
    if (!checkedCall(
            [this]()
            {
                return (bool) (m_clientSocket = SocketFactory::createStreamSocket());
            },
            "SocketFactory::createStreamSocket"))
    {
        return false;
    }

    return checkedCall(
            [this]() { return m_clientSocket->setNonBlockingMode(true); },
            "clientSocket::setNonBlockingMode");
}

template<typename F>
bool ConnectedSocketsSupplier::checkedCall(F f, const std::string& errorString)
{
    if (f())
        return true;

    m_lastError = errorString + " failed";
    return false;
}


bool ConnectedSocketsSupplier::connectSockets()
{
    if (!ok())
        return false;

    using namespace std::placeholders;
    m_acceptor->acceptAsync(std::bind(&ConnectedSocketsSupplier::onAccept, this, _1, _2));

    return connect();
}

bool ConnectedSocketsSupplier::connect()
{
    while (!m_clientSocket->connect(
                m_acceptor->getLocalAddress(),
                nx::network::deprecated::kDefaultConnectTimeout))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    m_lastError = m_connectedFuture.get();
    m_acceptor->pleaseStopSync();

    if (!ok())
        return false;

    return true;
}

void ConnectedSocketsSupplier::onAccept(
    SystemError::ErrorCode ecode,
    std::unique_ptr<AbstractStreamSocket> acceptedSocket)
{
    if (SystemError::noError != ecode)
    {
        m_connectedPromise.set_value("OnAccept error: " + std::to_string((int) ecode));
        return;
    }

    if (!acceptedSocket->setNonBlockingMode(true))
    {
        m_connectedPromise.set_value("OnAccept: acceptedSocket::setNonBlockingMode failed");
        return;
    }

    m_acceptedClientSocket = std::move(acceptedSocket);
    m_connectedPromise.set_value(std::string());
}

bool ConnectedSocketsSupplier::ok() const
{
    return m_lastError.empty();
}

std::unique_ptr<AbstractStreamSocket> ConnectedSocketsSupplier::clientSocket()
{
    return std::move(m_clientSocket);
}

std::unique_ptr<AbstractStreamSocket> ConnectedSocketsSupplier::serverSocket()
{
    return std::move(m_acceptedClientSocket);
}

std::string ConnectedSocketsSupplier::lastError()
{
    return m_lastError;
}

class P2PHttpTransport: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(m_connectedSocketsSupplier.connectSockets());
    }

// private:
    std::unique_ptr<P2PHttpServerTransport> m_server;
    std::unique_ptr<P2PHttpClientTransport> m_client;
    ConnectedSocketsSupplier m_connectedSocketsSupplier;
};

TEST_F(P2PHttpTransport, ClientReads)
{
    std::unique_ptr<nx::network::http::AsyncClient> clientHttpClient(new http::AsyncClient(m_connectedSocketsSupplier.clientSocket()));
    m_client.reset(new P2PHttpClientTransport(std::move(clientHttpClient)));

    nx::Buffer buf("hello");
    nx::Buffer serverReadBuf;
    serverReadBuf.reserve(100);
    auto serverSocket = m_connectedSocketsSupplier.serverSocket();
    //serverSocket->readSomeAsync(
    //    &serverReadBuf,
    //    [&](SystemError::ErrorCode , size_t )
    //    {
    //        NX_VERBOSE(this, lm("Server socket read: %1").args(serverReadBuf));
    //        m_server.reset(new P2PHttpServerTransport(std::move(serverSocket)));

    //        utils::MoveOnlyFunc<void(SystemError::ErrorCode, size_t)> onSend =
    //            [&](SystemError::ErrorCode, size_t)
    //            {
    //                m_server->sendAsync(buf, std::move(onSend));
    //            };

    //        m_server->sendAsync(
    //            buf,
    //            [&](SystemError::ErrorCode, size_t)
    //            {
    //                m_server->sendAsync(
    //                    buf,
    //                    [&](SystemError::ErrorCode, size_t)
    //                    {
    //                        qDebug() << "Sent two times";
    //                    });
    //            });
    //    });

    m_server.reset(new P2PHttpServerTransport(std::move(serverSocket)));

    utils::MoveOnlyFunc<void(SystemError::ErrorCode, size_t)> onSend =
        [&](SystemError::ErrorCode, size_t)
    {
        m_server->sendAsync(buf, std::move(onSend));
    };

    m_server->sendAsync(
        buf,
        [&](SystemError::ErrorCode, size_t)
    {
        m_server->sendAsync(
            buf,
            [&](SystemError::ErrorCode, size_t)
        {
            qDebug() << "Sent two times";
        });
    });

    utils::promise<void> p;
    auto f = p.get_future();

    nx::Buffer readBuf;
    m_client->readSomeAsync(
        &readBuf,
        [&](SystemError::ErrorCode error, size_t transferred)
        {
            qDebug() << "RECEIVED" << error << transferred;
            m_client->readSomeAsync(
                &readBuf,
                [&](SystemError::ErrorCode , size_t )
                {
                    qDebug() << "RECEIVED 2:" << readBuf;
                    p.set_value();
                });
        });

    f.wait();
}

} // namespace nx::network::test