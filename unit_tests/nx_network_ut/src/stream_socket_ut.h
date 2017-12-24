#pragma once

#include <gtest/gtest.h>

#include <nx/network/socket_common.h>
#include <nx/network/socket_factory.h>
#include <nx/network/system_socket.h>
#include <nx/utils/std/thread.h>

namespace nx {
namespace network {
namespace test {

namespace {

const int kIterations = 100;
const int kBufferSize = 1024 * 32;
const std::chrono::milliseconds kClientDelay(1);

} // namespace

template<typename SocketTypeSet>
class StreamSocket:
    public ::testing::Test
{
protected:
    void runTest(bool doServerDelay, bool doClientDelay)
    {
        using namespace std::chrono;

        nx::utils::thread serverThread(
            std::bind(&StreamSocket::serverMain, this, doServerDelay));

        typename SocketTypeSet::ClientSocket clientSocket;
        SocketAddress addr("127.0.0.1", m_serverPort.get_future().get());
        ASSERT_TRUE(clientSocket.connect(addr));
        if (doServerDelay)
            clientSocket.setSendTimeout(duration_cast<milliseconds>(kClientDelay).count());

        std::vector<int> buffer(kBufferSize);
        for (int i = 0; i < kBufferSize; ++i)
            buffer[i] = i;

        for (int i = 0; i < kIterations; ++i)
        {
            const int bufferSize = kBufferSize * sizeof(int);
            int offset = 0;
            while (offset < bufferSize)
            {
                int bytesSent = clientSocket.send((char *)buffer.data() + offset, bufferSize - offset);
                if (bytesSent > 0)
                {
                    offset += bytesSent;
                }
                else
                {
                    auto error = SystemError::getLastOSErrorCode();
                    if (error != SystemError::timedOut && error != SystemError::wouldBlock)
                    {
                        ASSERT_EQ(0, error);
                        break; //< Send error.
                    }
                }
            }
            if (doClientDelay)
                std::this_thread::sleep_for(kClientDelay);
        }
        clientSocket.close();
        serverThread.join();
    }

private:
    nx::utils::promise<int> m_serverPort;

    void serverMain(bool doServerDelay)
    {
        const auto server = std::make_unique<typename SocketTypeSet::ServerSocket>(
            SocketFactory::tcpClientIpVersion());

        ASSERT_TRUE(server->bind(SocketAddress::anyAddress));
        ASSERT_TRUE(server->listen());
        m_serverPort.set_value(server->getLocalAddress().port);

        std::unique_ptr<AbstractStreamSocket> client(server->accept());
        ASSERT_TRUE((bool)client);
        client->setRecvTimeout(kClientDelay);

        QByteArray wholeData;
        wholeData.reserve(kBufferSize * sizeof(int) * kIterations);
        std::vector<char> buffer(1024 * 1024);
        for (;;)
        {
            auto recv = client->recv(buffer.data(), (int)buffer.size());
            if (recv > 0)
            {
                wholeData.append(buffer.data(), recv);
            }
            else if (recv == 0)
            {
                break;
            }
            else
            {
                auto errCode = SystemError::getLastOSErrorCode();
                if (errCode != SystemError::timedOut && errCode != SystemError::again)
                {
                    ASSERT_EQ(0, errCode);
                    ASSERT_TRUE(client->isConnected());
                }
            }
            if (doServerDelay)
                std::this_thread::sleep_for(kClientDelay);
        }
        ASSERT_EQ((std::size_t)wholeData.size(), kBufferSize * sizeof(int) * kIterations);
        const int* testData = (const int*)wholeData.data();
        const int* endData = (const int*)(wholeData.data() + wholeData.size());
        int expectedValue = 0;
        while (testData < endData)
        {
            ASSERT_EQ(expectedValue, *testData);
            testData++;
            expectedValue = (expectedValue + 1) % kBufferSize;
        }
    }
};

TYPED_TEST_CASE_P(StreamSocket);

// Windows 8/10 doesn't support client->server send timeout.
// It close connection automatically with error 10053 after client send timeout.
TYPED_TEST_P(StreamSocket, DISABLED_receiveDelay)
{
    this->runTest(/*serverDelay*/ true, /*clientDelay*/ false);
}

TYPED_TEST_P(StreamSocket, sendDelay)
{
    this->runTest(/*serverDelay */ false, /*clientDelay*/ true);
}

REGISTER_TYPED_TEST_CASE_P(StreamSocket, DISABLED_receiveDelay, sendDelay);

} // namespace test
} // namespace network
} // namespace nx
