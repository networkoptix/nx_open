// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <thread>

#include <gtest/gtest.h>

#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QElapsedTimer>
#include <QtNetwork/QHostAddress>

#include <network/tcp_connection_priv.h>
#include <network/tcp_connection_processor.h>
#include <network/tcp_listener.h>
#include <nx/network/socket_common.h>
#include <nx/utils/test_support/test_options.h>
#include <nx/vms/common/test_support/test_context.h>
#include <recording/time_period_list.h>

namespace {
    static const int kDataTransferTimeout = 1000;
    static const int kTotalTestBytes = 1024 * 1024 * 20;
    static const int kTcpServerStartTimeoutMs = 1000;
}

class TestConnectionProcessor: public QnTCPConnectionProcessor
{
public:
    TestConnectionProcessor(
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        QnTcpListener* owner)
        QnTCPConnectionProcessor(std::move(socket), owner)
        :
    {
    }
    virtual ~TestConnectionProcessor() override
    {
        stop();
    }

protected:
    virtual void run() override
    {
        Q_D(QnTCPConnectionProcessor);
        ASSERT_TRUE(d->socket->setNonBlockingMode(true));
        ASSERT_TRUE(d->socket->setSendTimeout(kDataTransferTimeout));

        static const int kSteps = 64;
        std::vector<char> buffer(kTotalTestBytes / kSteps);
        for (int i = 0; i < kSteps; ++i)
            ASSERT_TRUE(sendBuffer(buffer.data(), (int) buffer.size()));
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
        std::unique_ptr<nx::network::AbstractStreamSocket> clientSocket) override
    {
        return new TestConnectionProcessor(std::move(clientSocket), this);
    }
};


TEST( TcpConnectionProcessor, sendAsyncData )
{
    nx::vms::common::test::Context context;

    // TcpListener uses commonModule()->moduleGuid().
    TestTcpListener tcpListener(context.commonModule(), QHostAddress::Any, 0);
    tcpListener.start();

    QElapsedTimer timer;
    timer.restart();
    while (!timer.hasExpired(kTcpServerStartTimeoutMs) && tcpListener.getPort() == 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto clientSocket = nx::network::SocketFactory::createStreamSocket(
        nx::network::ssl::kAcceptAnyCertificate, /*sslRequired*/ false);
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
