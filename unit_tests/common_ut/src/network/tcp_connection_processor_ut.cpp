#include <gtest/gtest.h>

#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QElapsedTimer>
#include <QtNetwork/QHostAddress>

#include <recording/time_period_list.h>
#include <nx/utils/test_support/test_options.h>
#include <network/tcp_connection_processor.h>
#include "nx/network/socket_common.h"
#include "network/tcp_listener.h"
#include "network/tcp_connection_priv.h"

namespace {
    static const int kDataTransferTimeout = 1000;
    static const int kTotalTestBytes = 1024 * 1024 * 20;
    static const int kTcpServerStartTimeoutMs = 1000;
}

class TestConnectionProcessor: public QnTCPConnectionProcessor
{
public:
    TestConnectionProcessor(QSharedPointer<AbstractStreamSocket> socket):
        QnTCPConnectionProcessor(socket)
    {
    }
    virtual ~TestConnectionProcessor() override
    {
        stop();
    }

protected:
    void TestConnectionProcessor::run()
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
    TestTcpListener(const QHostAddress& address, int port): QnTcpListener(address, port)
    {
    }
    virtual ~TestTcpListener() override
    {
        stop();
    }
protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(
        QSharedPointer<AbstractStreamSocket> clientSocket) override
    {
        return new TestConnectionProcessor(clientSocket);
    }
};


TEST( TcpConnectionProcessor, sendAsyncData )
{
    TestTcpListener tcpListener(QHostAddress::Any, 0);
    tcpListener.start();

    QElapsedTimer timer;
    timer.restart();
    while (!timer.hasExpired(kTcpServerStartTimeoutMs) && tcpListener.getPort() == 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto clientSocket = SocketFactory::createStreamSocket(false);
    clientSocket->setRecvTimeout(kDataTransferTimeout);
    clientSocket->connect(QLatin1String("127.0.0.1"), tcpListener.getPort());

    char buffer[kTotalTestBytes / 128];
    int gotBytes = 0;
    while (gotBytes < kTotalTestBytes)
    {
        int readed = clientSocket->recv(buffer, sizeof(buffer));
        ASSERT_TRUE(readed > 0);
        gotBytes += readed;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
