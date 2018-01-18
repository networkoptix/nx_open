#include <gtest/gtest.h>

#include <nx/network/address_resolver.h>
#include <nx/network/retry_timer.h>
#include <nx/network/system_socket.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/string.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/utils/scope_guard.h>

namespace nx {
namespace network {
namespace test {

namespace {

struct SocketContext
{
    std::unique_ptr<UDPSocket> socket;
    nx::Buffer readBuffer;
    nx::utils::promise<void> readPromise;
};

void onBytesRead(
    SocketContext* ctx,
    SystemError::ErrorCode /*errorCode*/,
    SocketAddress /*sourceEndpoint*/,
    size_t /*bytesRead*/)
{
    ctx->readPromise.set_value();
}

} // namespace

class UdpSocket:
    public ::testing::Test
{
public:
    UdpSocket():
        m_testMessage(QnUuid::createUuid().toSimpleString().toUtf8()),
        m_sender()
    {
    }

    ~UdpSocket()
    {
        if (m_sendRetryTimer)
        {
            m_sendRetryTimer->executeInAioThreadSync(
                [this]()
                {
                    m_sender.reset();
                    m_sendRetryTimer.reset();
                });
        }
        if (m_receiver)
            m_receiver->pleaseStopSync();
    }

protected:
    void udpSocketTransferTest(
        int ipVersion,
        const HostAddress& hostNameMappedToLocalHost)
    {
        initializeSender(ipVersion);
        initializeReceiver(ipVersion);

        startSending(
            SocketAddress(hostNameMappedToLocalHost, m_receiver->getLocalAddress().port));

        assertMessageIsReceivedFrom(
            ipVersion,
            SocketAddress("127.0.0.1", m_sender->getLocalAddress().port));

        assertSendResultIsCorrect(
            SocketAddress("127.0.0.1", m_receiver->getLocalAddress().port));
    }

private:
    struct SendResult
    {
        SystemError::ErrorCode code;
        SocketAddress resolvedTargetEndpoint;
        std::size_t size;
    };

    const Buffer m_testMessage;
    std::unique_ptr<UDPSocket> m_sender;
    std::unique_ptr<UDPSocket> m_receiver;
    nx::utils::SyncQueue<SendResult> m_successfulSendResultQueue;
    std::unique_ptr<RetryTimer> m_sendRetryTimer;

    void initializeSender(int ipVersion)
    {
        m_sender = std::make_unique<UDPSocket>(ipVersion);
        ASSERT_TRUE(m_sender->bind(SocketAddress::anyPrivateAddress));
    }

    void initializeReceiver(int ipVersion)
    {
        m_receiver = std::make_unique<UDPSocket>(ipVersion);
        ASSERT_TRUE(m_receiver->bind(SocketAddress::anyPrivateAddress));
    }

    void startSending(const SocketAddress& targetEndpoint)
    {
        ASSERT_TRUE(m_sender->setNonBlockingMode(true));
        m_sendRetryTimer = std::make_unique<RetryTimer>(RetryPolicy(
            RetryPolicy::kInfiniteRetries,
            std::chrono::milliseconds(100),
            1,
            RetryPolicy::kNoMaxDelay));
        m_sendRetryTimer->bindToAioThread(m_sender->getAioThread());

        issueSendTo(targetEndpoint);
    }

    void issueSendTo(const SocketAddress& targetEndpoint)
    {
        m_sender->sendToAsync(
            m_testMessage,
            targetEndpoint,
            [this, targetEndpoint](
                SystemError::ErrorCode code,
                SocketAddress resolvedTargetEndpoint,
                size_t size)
            {
                if (code == SystemError::noError)
                {
                    m_successfulSendResultQueue.push(
                        SendResult{code, resolvedTargetEndpoint, size});
                }

                m_sendRetryTimer->scheduleNextTry(
                    [this, targetEndpoint]()
                    {
                        issueSendTo(targetEndpoint);
                    });
            });
    }

    void assertMessageIsReceivedFrom(
        int ipVersion,
        const SocketAddress& senderEndpoint)
    {
        Buffer buffer;
        buffer.resize(1024);
        SocketAddress remoteEndpoint;
        ASSERT_EQ(
            m_testMessage.size(),
            m_receiver->recvFrom(buffer.data(), buffer.size(), &remoteEndpoint));
        ASSERT_EQ(m_testMessage, buffer.left(m_testMessage.size()));

        ASSERT_TRUE(remoteEndpoint.address.isIpAddress());
        ASSERT_EQ(senderEndpoint.toString(), remoteEndpoint.toString());

        if (ipVersion == AF_INET)
        {
            ASSERT_EQ(
                senderEndpoint.address.ipV4()->s_addr,
                remoteEndpoint.address.ipV4()->s_addr);
        }
        else if (ipVersion == AF_INET6)
        {
            ASSERT_TRUE(memcmp(
                &senderEndpoint.address.ipV6().first.get(),
                &remoteEndpoint.address.ipV6().first.get(),
                sizeof(in6_addr)) == 0);
        }
        else
        {
            FAIL();
        }
    }

    void assertSendResultIsCorrect(const SocketAddress& receiverEndpoint)
    {
        const auto sendResult = m_successfulSendResultQueue.pop();
        ASSERT_EQ(SystemError::noError, sendResult.code)
            << SystemError::toString(sendResult.code).toStdString();
        ASSERT_TRUE(sendResult.resolvedTargetEndpoint.address.isIpAddress());
        ASSERT_EQ(receiverEndpoint.toString(), sendResult.resolvedTargetEndpoint.toString());
        ASSERT_EQ((size_t)m_testMessage.size(), sendResult.size);
    }
};

TEST_F(UdpSocket, TransferIpV4) { udpSocketTransferTest(AF_INET, "127.0.0.1"); }
TEST_F(UdpSocket, TransferDnsIpV4) { udpSocketTransferTest(AF_INET, "localhost"); }
TEST_F(UdpSocket, TransferIpV6) { udpSocketTransferTest(AF_INET6, "127.0.0.1"); }
TEST_F(UdpSocket, TransferDnsIpV6) { udpSocketTransferTest(AF_INET6, "localhost"); }

TEST_F(UdpSocket, TransferNat64)
{
    HostAddress ip("12.34.56.78");
    SocketGlobals::addressResolver().dnsResolver().addEtcHost(
        ip.toString(), {HostAddress::localhost});

    udpSocketTransferTest(AF_INET6, ip);
    SocketGlobals::addressResolver().dnsResolver().removeEtcHost(ip.toString());
}

TEST_F(UdpSocket, DISABLED_multipleSocketsOnTheSamePort)
{
    constexpr int socketCount = 2;

    std::vector<SocketContext> sockets;
    const auto socketsCleanupGuard = makeScopeGuard(
        [&sockets]()
        {
            for (auto& ctx: sockets)
                ctx.socket->pleaseStopSync();
        });

    for (std::size_t i = 0; i < socketCount; ++i)
    {
        sockets.push_back(SocketContext());

        sockets[i].socket = std::make_unique<UDPSocket>();
        sockets[i].readBuffer.reserve(1024);
        ASSERT_TRUE(sockets[i].socket->setReuseAddrFlag(true));
        ASSERT_TRUE(sockets[i].socket->setNonBlockingMode(true));
        ASSERT_TRUE(sockets[i].socket->setRecvTimeout(0));
        if (i == 0)
        {
            ASSERT_TRUE(sockets[i].socket->bind(SocketAddress(HostAddress::localhost, 0)));
        }
        else
        {
            ASSERT_TRUE(sockets[i].socket->bind(sockets[0].socket->getLocalAddress()));
        }

        using namespace std::placeholders;
        sockets[i].socket->recvFromAsync(
            &sockets[i].readBuffer,
            std::bind(&onBytesRead, &sockets[i], _1, _2, _3));
    }

    constexpr const char* testMessage = "bla-bla-bla";
    UDPSocket sendingSocket;
    ASSERT_TRUE(
        sendingSocket.sendTo(
            testMessage,
            strlen(testMessage),
            sockets[0].socket->getLocalAddress()));

    for (auto& ctx: sockets)
    {
        ASSERT_EQ(
            std::future_status::ready,
            ctx.readPromise.get_future().wait_for(std::chrono::seconds(1)));
        ASSERT_EQ(testMessage, ctx.readBuffer);
    }
}

TEST_F(UdpSocket, DISABLED_Performance)
{
    const uint64_t kBufferSize = 1500;
    const uint64_t kTransferSize = uint64_t(10) * 1024 * 1024 * 1024;

    UDPSocket server(AF_INET);
    server.bind(SocketAddress::anyPrivateAddress);
    const auto address = server.getLocalAddress();
    NX_LOG(lm("%1").arg(address), cl_logINFO);
    nx::utils::thread serverThread(
        [&]()
        {
            const auto startTime = std::chrono::steady_clock::now();
            Buffer buffer((int) kBufferSize, Qt::Uninitialized);

            int recv = 0;
            uint64_t transferSize = 0;
            uint64_t transferCount = 0;
            while (transferSize < kTransferSize)
            {
                recv = server.recv(buffer.data(), buffer.size(), 0);
                if (recv <= 0)
                    break;

                transferSize += (uint64_t) recv;
                transferCount += 1;
            }

            const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime);

            NX_LOG(lm("Resieve ended (%1): %2")
                .args(recv, SystemError::getLastOSErrorText()), cl_logINFO);

            const auto bytesPerS = double(transferSize) * 1000 / durationMs.count();
            NX_LOG(lm("Resieved size=%1b, count=%2, average=%3, duration=%4, speed=%5bps")
                .args(nx::utils::bytesToString(transferSize), transferCount,
                    nx::utils::bytesToString(transferSize / transferCount),
                    durationMs, nx::utils::bytesToString((uint64_t) bytesPerS)), cl_logINFO);
        });

    nx::utils::thread clientThread(
        [&]()
        {
            UDPSocket client(AF_INET);
            client.setDestAddr(address);
            Buffer buffer((int) kBufferSize, 'X');
            int send = 0;
            uint64_t transferSize = 0;
            while (transferSize < kTransferSize + kTransferSize / 10)
            {
                send = client.send(buffer.data(), buffer.size());
                if (send <= 0)
                    break;

                transferSize += (uint64_t) send;
            }
        });

    serverThread.join();
    clientThread.join();
}

} // namespace test
} // namespace network
} // namespace nx
