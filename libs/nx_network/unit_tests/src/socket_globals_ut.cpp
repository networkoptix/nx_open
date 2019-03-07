#include <atomic>

#include <gtest/gtest.h>

#include <nx/network/socket_global.h>
#include <nx/network/system_socket.h>

namespace nx::network::test {

namespace {

class TestSocket:
    public TCPSocket
{
    using base_type = TCPSocket;

public:
    TestSocket(std::atomic<int>* count):
        base_type(AF_INET),
        m_count(count)
    {
        m_count->fetch_add(1);
    }

    ~TestSocket()
    {
        m_count->fetch_add(-1);
    }

private:
    std::atomic<int>* m_count = nullptr;
};

} // namespace

//-------------------------------------------------------------------------------------------------

class SocketGlobals:
    public ::testing::Test
{
public:
    SocketGlobals():
        m_aioThread(nx::network::SocketGlobals::instance().aioService().getRandomAioThread())
    {
    }

    ~SocketGlobals()
    {
        if (m_deinitialized)
            SocketGlobalsHolder::instance()->initialize(/*initializePeerId*/ true);
    }

protected:
    void addSomeSockets()
    {
        constexpr int count = 2;
        for (int i = 0; i < count; ++i)
        {
            m_sockets.push_back(SocketContext());
            m_sockets.back().socket = std::make_unique<TestSocket>(&m_count);
        }
    }

    void scheduleAsyncOperations()
    {
        m_serverSocket = std::make_unique<TCPServerSocket>(AF_INET);
        ASSERT_TRUE(m_serverSocket->bind(SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(m_serverSocket->listen());

        for (auto& ctx: m_sockets)
        {
            ASSERT_TRUE(ctx.socket->connect(m_serverSocket->getLocalAddress(), kNoTimeout));
            ASSERT_TRUE(ctx.socket->setNonBlockingMode(true));

            ctx.socket->bindToAioThread(m_aioThread);
            ctx.readBuf.reserve(128);
            ctx.socket->readSomeAsync(&ctx.readBuf, [this](auto... /*args*/){});
        }
    }

    void passSocketsOwnershipToAio()
    {
        post(std::exchange(m_sockets, {}));
    }

    void whenDeinitialize()
    {
        SocketGlobalsHolder::instance()->uninitialize();
        m_deinitialized = true;
    }

    void thenAllSocketsAreRemoved()
    {
        ASSERT_EQ(0, m_count);
    }

private:
    struct SocketContext
    {
        std::unique_ptr<TestSocket> socket;
        nx::Buffer readBuf;
    };

    std::vector<SocketContext> m_sockets;
    std::atomic<int> m_count = 0;
    bool m_deinitialized = false;
    std::unique_ptr<TCPServerSocket> m_serverSocket;
    nx::network::aio::AbstractAioThread* m_aioThread = nullptr;

    void post(std::vector<SocketContext> sockets)
    {
        m_aioThread->post(
            /*pollable*/ nullptr,
            [this, sockets = std::move(sockets)]() mutable
            {
                post(std::move(sockets));
            });
    }
};

TEST_F(SocketGlobals, does_not_crash_when_deinitializing_with_sockets_present)
{
    addSomeSockets();
    scheduleAsyncOperations();
    passSocketsOwnershipToAio();

    whenDeinitialize();

    thenAllSocketsAreRemoved();
}

} // namespace nx::network::test
