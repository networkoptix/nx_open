#include "test_setup.h"

#include <nx/utils/test_support/sync_queue.h>
#include <nx/network/test_support/socket_test_helper.h>
#include <nx/utils/random.h>

#include <QNetworkProxy>
#include <QTcpSocket>

namespace nx {
namespace cloud {
namespace gateway {
namespace test {

const constexpr int kTimeoutMsec = 10;

class VmsGatewayConnectTest:
    public BasicComponentTest
{
public:
    VmsGatewayConnectTest():
        server(
            network::test::TestTrafficLimitType::outgoing,
            network::test::TestConnection::kReadBufferSize,
            network::test::TestTransmissionMode::pong)
    {
        NX_CRITICAL(server.start());

        m_QtThread = nx::utils::thread(
            [this]() {
                int argc = 1;
                NX_VERBOSE(this, "Running QCoreApplication");
                QCoreApplication a(argc, nullptr);
                a.exec();
                NX_VERBOSE(this, "Finished QCoreApplication::exec()");
            });
    }

    ~VmsGatewayConnectTest()
    {
        qApp->exit();
        m_QtThread.join();
    }


    std::unique_ptr<QTcpSocket> connectProxySocket()
    {
        auto socket = std::make_unique<QTcpSocket>();
        socket->setProxy(QNetworkProxy(
            QNetworkProxy::HttpProxy,
            endpoint().address.toString(),
            endpoint().port));

        const auto addr = server.addressBeingListened();

        // FIXME: "Network proxy is not used if the address used in connectToHost(), bind() or
        //       listen() is equivalent to QHostAddress::LocalHost or QHostAddress::LocalHostIPv6"
        //       (but Qt on linux has another opinion about this and works well...).
        // see: http://doc.qt.io/qt-5/qnetworkproxy.html
        socket->connectToHost(addr.address.toString(), addr.port);
        return socket;
    }

    network::test::RandomDataTcpServer server;

private:
    nx::utils::thread m_QtThread;
};


// FIXME: Should fix waitForConnected and waitForReadyRead functions
//        "This function may fail randomly on Windows. Consider using the event loop and the
//        connected() signal if your software will run on Windows."
// See: http://doc.qt.io/qt-5/qabstractsocket.html#waitForConnected.

TEST_F(VmsGatewayConnectTest, connectionClose)
{
    ASSERT_TRUE(startAndWaitUntilStarted(true, false, true));

    utils::TestSyncQueue<bool> waitQueue{std::chrono::milliseconds(kTimeoutMsec)};
    server.setOnFinishedConnectionHandler(
        [&waitQueue](network::test::TestConnection*)
        {
            waitQueue.push(true);
        });

    // Client closes connection.
    auto clientSocket = connectProxySocket();
    ASSERT_TRUE(clientSocket->waitForConnected(kTimeoutMsec))
        << "Connect failed: " << clientSocket->errorString().toStdString();

    clientSocket->close();
    waitQueue.pop();
    ASSERT_EQ(server.statistics().onlineConnections, 0);

    clientSocket.reset();

    // Server closes connection.
    clientSocket = connectProxySocket();
    ASSERT_TRUE(clientSocket->waitForConnected(kTimeoutMsec))
        << "Connect failed: " << clientSocket->errorString().toStdString();

    server.pleaseStopSync();
    ASSERT_FALSE(clientSocket->waitForReadyRead(kTimeoutMsec));
    ASSERT_EQ(clientSocket->error(), QAbstractSocket::RemoteHostClosedError);
}

TEST_F(VmsGatewayConnectTest, IpSpecified)
{
    ASSERT_TRUE(startAndWaitUntilStarted(true, false, true));
    const auto clientSocket = connectProxySocket();

    ASSERT_TRUE(clientSocket->waitForConnected(kTimeoutMsec))
        << "Connect failed: " << clientSocket->errorString().toStdString();

    QByteArray writeData(utils::random::generate(
        network::test::TestConnection::kReadBufferSize));
    ASSERT_EQ(clientSocket->write(writeData), writeData.size());

    ASSERT_TRUE(clientSocket->waitForReadyRead(kTimeoutMsec))
        << "ReadyRead failed: " << clientSocket->errorString().toStdString();
    server.pleaseStopSync();

    const auto readData = clientSocket->readAll();
    ASSERT_GT(readData.size(), 0);
    ASSERT_TRUE(writeData.startsWith(readData));
}

// Tests correct handling of CONNECT request when client sends data right after request without
// waiting of response.
TEST_F(VmsGatewayConnectTest, httpPipelining)
{
    ASSERT_TRUE(startAndWaitUntilStarted(true, false, true));

    const QByteArray connectRequest(QString(
        "CONNECT %1 HTTP/1.1\r\n"
        "Host: localhost\r\n\r\n"
        ).arg(server.addressBeingListened().toString()).toStdString().c_str());
    const QByteArray dataAfterRequest("Some data not related to http\n");
    const QByteArray connectResponse("HTTP/1.1 200 Connection estabilished\r\n\r\n");

    server.setConnectionsReadBufferSize(dataAfterRequest.size());

    nx::network::TCPSocket clientSocket;
    clientSocket.setRecvTimeout(kTimeoutMsec);
    ASSERT_TRUE(clientSocket.connect(endpoint(), std::chrono::milliseconds(kTimeoutMsec)))
        << "Connect failed: " << SystemError::getLastOSErrorText().toStdString();


    // Send CONNECT request and other data right after it.
    ASSERT_EQ(clientSocket.send(connectRequest + dataAfterRequest,
        connectRequest.size() + dataAfterRequest.size()),
        connectRequest.size() + dataAfterRequest.size());

    QByteArray responseReceiveBuffer;
    responseReceiveBuffer.resize(connectResponse.size());
    QByteArray dataAfterRequestReceiveBuffer;
    dataAfterRequestReceiveBuffer.resize(dataAfterRequest.size());

    // Receive connect response.
    ASSERT_EQ(clientSocket.recv(responseReceiveBuffer.data(), responseReceiveBuffer.size(), 0),
        responseReceiveBuffer.size());
    ASSERT_EQ(connectResponse, responseReceiveBuffer);

    // Receive data after connect response.
    ASSERT_EQ(clientSocket.recv(dataAfterRequestReceiveBuffer.data(),
        dataAfterRequestReceiveBuffer.size(), 0), dataAfterRequestReceiveBuffer.size());
    ASSERT_EQ(dataAfterRequest, dataAfterRequestReceiveBuffer);

    // Graceful shutdown.
    clientSocket.shutdown();
    server.pleaseStopSync();
}

TEST_F(VmsGatewayConnectTest, ConnectNotSupported)
{
    ASSERT_TRUE(startAndWaitUntilStarted(true, false, false));
    const auto socket = connectProxySocket();
    ASSERT_FALSE(socket->waitForConnected());
    server.pleaseStopSync();
}

TEST_F(VmsGatewayConnectTest, IpForbidden)
{
    ASSERT_TRUE(startAndWaitUntilStarted(false, false, true));
    const auto socket = connectProxySocket();
    ASSERT_FALSE(socket->waitForConnected());
    server.pleaseStopSync();
}

} // namespace test
} // namespace gateway
} // namespace cloud
} // namespace nx
