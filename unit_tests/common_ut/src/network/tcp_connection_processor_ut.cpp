#include <gtest/gtest.h>

#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QElapsedTimer>
#include <QtNetwork/QHostAddress>

#include <common/static_common_module.h>
#include <common/common_module.h>

#include <nx/core/access/access_types.h>

#include <recording/time_period_list.h>
#include <nx/utils/test_support/test_options.h>
#include <network/tcp_connection_processor.h>
#include <nx/network/socket_common.h>
#include <network/tcp_listener.h>
#include <network/tcp_connection_priv.h>
#include <thread>

namespace {
    static const int kDataTransferTimeout = 1000;
    static const int kTotalTestBytes = 1024 * 1024 * 20;
    static const int kTcpServerStartTimeoutMs = 1000;
}

class TestConnectionProcessor: public QnTCPConnectionProcessor
{
public:
    TestConnectionProcessor(
        QSharedPointer<nx::network::AbstractStreamSocket> socket,
        QnTcpListener* owner)
    :
        QnTCPConnectionProcessor(socket, owner)
    {
    }
    virtual ~TestConnectionProcessor() override
    {
        stop();
    }

protected:
    void run()
    {
        Q_D(QnTCPConnectionProcessor);
        ASSERT_TRUE(d->socket->setNonBlockingMode(true));
        ASSERT_TRUE(d->socket->setSendTimeout(kDataTransferTimeout));

        static const int kSteps = 64;
        std::vector<char> buffer(kTotalTestBytes / kSteps);
        for (int i = 0; i < kSteps; ++i)
            ASSERT_TRUE(sendData(buffer.data(), buffer.size()));
    }

};

class TestTcpListener: public QnTcpListener
{
public:
    TestTcpListener(QnCommonModule* commonModule, const QHostAddress& address, int port):
        QnTcpListener(commonModule, address, port, DEFAULT_MAX_CONNECTIONS, /*useSSL*/ false)
    {
    }

    virtual ~TestTcpListener() override
    {
        stop();
    }

protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(
        QSharedPointer<nx::network::AbstractStreamSocket> clientSocket) override
    {
        return new TestConnectionProcessor(clientSocket, this);
    }
};


TEST( TcpConnectionProcessor, sendAsyncData )
{
    QnStaticCommonModule staticCommon(Qn::PT_NotDefined, QString(), QString());

    // TcpListener uses commonModule()->moduleGuid().
    QnCommonModule commonModule(false, nx::core::access::Mode::direct);

    TestTcpListener tcpListener(&commonModule, QHostAddress::Any, 0);
    tcpListener.start();

    QElapsedTimer timer;
    timer.restart();
    while (!timer.hasExpired(kTcpServerStartTimeoutMs) && tcpListener.getPort() == 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto clientSocket = nx::network::SocketFactory::createStreamSocket(false);
    ASSERT_TRUE(clientSocket->setRecvTimeout(kDataTransferTimeout));
    ASSERT_TRUE(clientSocket->connect(
        nx::network::SocketAddress(nx::network::HostAddress::localhost, tcpListener.getPort()),
        nx::network::kNoTimeout));

    char buffer[kTotalTestBytes / 128];
    int gotBytes = 0;
    while (gotBytes < kTotalTestBytes)
    {
        int bytesRead = clientSocket->recv(buffer, sizeof(buffer));
        ASSERT_TRUE(bytesRead > 0);
        gotBytes += bytesRead;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
