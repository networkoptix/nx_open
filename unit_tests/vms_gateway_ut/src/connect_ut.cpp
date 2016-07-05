#include "test_setup.h"

#include <nx/network/test_support/socket_test_helper.h>
#include <nx/utils/random.cpp>

#include <QTcpSocket>
#include <QNetworkProxy>

namespace nx {
namespace cloud {
namespace gateway {
namespace test {

class VmsGatewayConnectTest
:
    public VmsGatewayFunctionalTest
{
public:
    VmsGatewayConnectTest()
    :
      server(
          network::test::TestTrafficLimitType::outgoing,
          network::test::TestConnection::kReadBufferSize,
          network::test::TestTransmissionMode::pong)
    {
        NX_CRITICAL(server.start());
    }

    std::unique_ptr<QTcpSocket> connectProxySocket()
    {
        auto socket = std::make_unique<QTcpSocket>();
        socket->setProxy(QNetworkProxy(
            QNetworkProxy::HttpProxy,
            endpoint().address.toString(),
            endpoint().port));

        const auto addr = server.addressBeingListened();
        socket->connectToHost(addr.address.toString(), addr.port);
        return socket;
    }

    network::test::RandomDataTcpServer server;
};

TEST_F(VmsGatewayConnectTest, IpSpecified)
{
    ASSERT_TRUE(startAndWaitUntilStarted(true, false, true));

    const auto socket = connectProxySocket();
    ASSERT_TRUE(socket->waitForConnected());

    QByteArray writeData(utils::generateRandomData(
        network::test::TestConnection::kReadBufferSize));
    ASSERT_EQ(socket->write(writeData), writeData.size());

    ASSERT_TRUE(socket->waitForReadyRead());
    server.pleaseStopSync();

    const auto readData = socket->readAll();
    ASSERT_GT(readData.size(), 0);
    ASSERT_TRUE(writeData.startsWith(readData));
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

}   // namespace test
}   // namespace gateway
}   // namespace cloud
}   // namespace nx
