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


const constexpr int kTimeoutMsec = 100;
const QByteArray successResponse = "HTTP/1.1 200 OK\r\n";

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
    }

    // NOTE: Socket is out parameter, as gtest does not support assertions in non-void functions.
    void connectProxySocket(const network::SocketAddress serverAddress,
        std::unique_ptr<network::TCPSocket>& socket,
        const QByteArray &connectResponse = successResponse)
    {
        socket = std::make_unique<network::TCPSocket>();
        socket->setRecvTimeout(kTimeoutMsec);

        // Connect to proxy.
        ASSERT_TRUE(socket->connect(endpoint(), std::chrono::milliseconds(kTimeoutMsec)))
            << "Connect failed: " << SystemError::getLastOSErrorText().toStdString();

        const QByteArray connectRequest(QString(
            "CONNECT %1 HTTP/1.1\r\n"
            "Host: localhost\r\n\r\n"
            ).arg(serverAddress.toString()).toStdString().c_str());

        // Send CONNECT request to proxy.
        ASSERT_EQ(socket->send(connectRequest, connectRequest.size()), connectRequest.size());

        QByteArray responseReceiveBuffer;
        responseReceiveBuffer.resize(connectResponse.size());

        // Check that CONNECT was successfull.
        ASSERT_EQ(socket->recv(responseReceiveBuffer.data(), responseReceiveBuffer.size(), 0),
            responseReceiveBuffer.size());
        // We want to check only the first response line.
        ASSERT_TRUE(responseReceiveBuffer.startsWith(connectResponse));

        // Clean http options which can be left in socket buffer.
        while(socket->recv(responseReceiveBuffer.data(), responseReceiveBuffer.size(),
            MSG_DONTWAIT) > 0);
        ASSERT_EQ(SystemError::getLastOSErrorCode(), SystemError::again);
    }

    network::test::RandomDataTcpServer server;
};


TEST_F(VmsGatewayConnectTest, ConnectionClose)
{
    ASSERT_TRUE(startAndWaitUntilStarted(true, false, true));

    utils::TestSyncQueue<bool> waitQueue{std::chrono::milliseconds(kTimeoutMsec)};
    server.setOnFinishedConnectionHandler(
        [&waitQueue](network::test::TestConnection*)
        {
            waitQueue.push(true);
        });

    // Client closes connection.
    std::unique_ptr<network::TCPSocket> clientSocket;
    connectProxySocket(server.addressBeingListened(), clientSocket);

    clientSocket->close();
    waitQueue.pop();
    ASSERT_EQ(server.statistics().onlineConnections, 0);

    clientSocket.reset();

    // Server closes connection.
    connectProxySocket(server.addressBeingListened(), clientSocket);

    server.pleaseStopSync();
    QByteArray receiveBuffer(1, 0);  //< It should be >0 to differentiate EOF and zero bytes read.
    ASSERT_EQ(clientSocket->recv(receiveBuffer.data(), receiveBuffer.size(), 0), 0);
}

TEST_F(VmsGatewayConnectTest, IpSpecified)
{
    ASSERT_TRUE(startAndWaitUntilStarted(true, false, true));

    std::unique_ptr<network::TCPSocket> clientSocket;
    connectProxySocket(server.addressBeingListened(), clientSocket);

    QByteArray writeData(utils::random::generate(
        network::test::TestConnection::kReadBufferSize));
    ASSERT_EQ(clientSocket->send(writeData, writeData.size()), writeData.size());

    QByteArray receiveBuffer;
    receiveBuffer.resize(writeData.size());
    ASSERT_EQ(clientSocket->recv(receiveBuffer.data(), receiveBuffer.size(), 0),
        receiveBuffer.size());

    server.pleaseStopSync();

    ASSERT_EQ(receiveBuffer, writeData);
}

TEST_F(VmsGatewayConnectTest, ConcurrentConnections)
{
    ASSERT_TRUE(startAndWaitUntilStarted(true, false, true));

    std::unique_ptr<network::TCPSocket> clientSocketFirst;
    connectProxySocket(server.addressBeingListened(), clientSocketFirst);
    std::unique_ptr<network::TCPSocket> clientSocketSecond;
    connectProxySocket(server.addressBeingListened(), clientSocketSecond);

    QByteArray writeData(utils::random::generate(
        network::test::TestConnection::kReadBufferSize));
    ASSERT_EQ(clientSocketFirst->send(writeData, writeData.size()), writeData.size());
    ASSERT_EQ(clientSocketSecond->send(writeData, writeData.size()), writeData.size());

    QByteArray receiveBuffer;
    receiveBuffer.resize(writeData.size());
    ASSERT_EQ(clientSocketFirst->recv(receiveBuffer.data(), receiveBuffer.size(), 0),
        receiveBuffer.size());
    ASSERT_EQ(clientSocketSecond->recv(receiveBuffer.data(), receiveBuffer.size(), 0),
        receiveBuffer.size());
    server.pleaseStopSync();
}

TEST_F(VmsGatewayConnectTest, WrongAddressConnect)
{
    ASSERT_TRUE(startAndWaitUntilStarted(true, false, true));
    std::unique_ptr<network::TCPSocket> clientSocket;
    connectProxySocket("127.0.0.1:1", clientSocket, "HTTP/1.1 503 Service Unavailable\r\n");
    server.pleaseStopSync();
}

TEST_F(VmsGatewayConnectTest, ConnectNotSupported)
{
    ASSERT_TRUE(startAndWaitUntilStarted(true, false, false));
    std::unique_ptr<network::TCPSocket> clientSocket;
    connectProxySocket(server.addressBeingListened(), clientSocket, "HTTP/1.1 403 Forbidden\r\n");
    server.pleaseStopSync();
}

TEST_F(VmsGatewayConnectTest, IpForbidden)
{
    ASSERT_TRUE(startAndWaitUntilStarted(false, false, true));
    std::unique_ptr<network::TCPSocket> clientSocket;
    connectProxySocket(server.addressBeingListened(), clientSocket, "HTTP/1.1 403 Forbidden\r\n");
    server.pleaseStopSync();
}

} // namespace test
} // namespace gateway
} // namespace cloud
} // namespace nx
