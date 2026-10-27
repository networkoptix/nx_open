// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <array>
#include <atomic>
#include <cstdlib>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

#ifndef _WIN32
    #include <fcntl.h>
    #include <signal.h>
    #include <spawn.h>
    #include <sys/mman.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

#include <gtest/gtest.h>

#include <nx/network/system_socket.h>
#include <nx/network/test_support/simple_socket_test_helper.h>

#ifndef _WIN32
extern "C" char** environ;
#endif

namespace nx::network::test {

class TcpSocket:
    public ::testing::Test
{
public:
    ~TcpSocket()
    {
        if (m_client)
            m_client->pleaseStopSync();

#ifndef _WIN32
        if (m_childPid > 0)
        {
            ::kill(m_childPid, SIGKILL);
            ::waitpid(m_childPid, nullptr, 0);
        }
#endif
    }

protected:
    SocketAddress serverAddress() const { return m_tcpServerSocket.getLocalAddress(); }

    void whenAcceptedSocket()
    {
        m_client = std::make_unique<TCPSocket>(AF_INET);
        ASSERT_TRUE(m_client->setNonBlockingMode(true));

        std::promise<SystemError::ErrorCode> connected;
        m_client->connectAsync(
            m_tcpServerSocket.getLocalAddress(),
            [&connected](SystemError::ErrorCode resultCode)
            {
                connected.set_value(resultCode);
            });

         m_acceptedSocket = m_tcpServerSocket.accept();
         ASSERT_EQ(SystemError::noError, connected.get_future().get());
    }

#ifndef _WIN32
    void thenAcceptedSocketHasCloseOnExecFlag()
    {
        const int flags = ::fcntl(m_acceptedSocket->handle(), F_GETFD);
        ASSERT_NE(-1, flags);
        ASSERT_TRUE(flags & FD_CLOEXEC);
    }

    // The child is spawned without any descriptor filtering (unlike QProcess with
    // UnixProcessFlag::CloseFileDescriptors): only the close-on-exec flag of the accepted
    // socket prevents inheriting it.
    void whenSpawnLongLivingChildProcess()
    {
        const char* argv[] = {"/bin/sleep", "60", nullptr};
        ASSERT_EQ(0,
            ::posix_spawn(
                &m_childPid, argv[0], nullptr, nullptr, const_cast<char* const*>(argv), environ));
    }

    void whenCloseAcceptedSocket() { m_acceptedSocket.reset(); }

    void thenClientReceivesEofImmediately()
    {
        ASSERT_TRUE(m_client->setNonBlockingMode(false));
        ASSERT_TRUE(m_client->setRecvTimeout(3000));

        char buf[1];
        ASSERT_EQ(0, m_client->recv(buf, sizeof(buf), 0)) << SystemError::getLastOSErrorText();
    }
#endif

    void thenTimeoutsAreReportedCorrectly()
    {
        thenRecvTimeoutIsReportedCorrectly();
        thenSendTimeoutIsReportedCorrectly();
    }

    void thenRecvTimeoutIsReportedCorrectly()
    {
        unsigned int reportedTimeout = 0;
        ASSERT_TRUE(m_acceptedSocket->getRecvTimeout(&reportedTimeout));

        unsigned int actualTimeout = 0;
        ASSERT_TRUE(readSocketTimeout(m_acceptedSocket->handle(), SO_RCVTIMEO, &actualTimeout));

        ASSERT_EQ(actualTimeout, reportedTimeout);
    }

    void thenSendTimeoutIsReportedCorrectly()
    {
        unsigned int reportedTimeout = 0;
        ASSERT_TRUE(m_acceptedSocket->getSendTimeout(&reportedTimeout));

        unsigned int actualTimeout = 0;
        ASSERT_TRUE(readSocketTimeout(m_acceptedSocket->handle(), SO_SNDTIMEO, &actualTimeout));

        ASSERT_EQ(actualTimeout, reportedTimeout);
    }

private:
    std::unique_ptr<TCPSocket> m_client;
    TCPServerSocket m_tcpServerSocket;
    std::unique_ptr<AbstractStreamSocket> m_acceptedSocket;
#ifndef _WIN32
    pid_t m_childPid = 0;
#endif

    virtual void SetUp() override
    {
        ASSERT_TRUE(m_tcpServerSocket.setRecvTimeout(
            nx::utils::random::number<unsigned int>(10000, 90000)));
        ASSERT_TRUE(m_tcpServerSocket.setSendTimeout(
            nx::utils::random::number<unsigned int>(10000, 90000)));

        ASSERT_TRUE(m_tcpServerSocket.bind(SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(m_tcpServerSocket.listen());
    }

    static bool readSocketTimeout(int fd, int socketOption, unsigned int* millis)
    {
        #ifdef _WIN32
            int valLen = sizeof(*millis);
            if (getsockopt(fd, SOL_SOCKET, socketOption, (char*)millis, &valLen) != 0)
                return false;
        #else
            timeval tv;
            memset(&tv, 0, sizeof(tv));
            socklen_t valLen = sizeof(tv);
            if (::getsockopt(fd, SOL_SOCKET, socketOption, (void*)&tv, &valLen) < 0)
                return false;
            *millis = tv.tv_sec*1000 + tv.tv_usec/1000;
        #endif

        return true;
    }
};

TEST_F(TcpSocket, KeepAliveOptions )
{
    if( SocketFactory::isStreamSocketTypeEnforced() )
        return;

    const auto socket = std::make_unique< TCPSocket >(AF_INET);
    std::optional< KeepAliveOptions > result;

    // Enable
    typedef std::chrono::seconds seconds;
    ASSERT_TRUE( socket->setKeepAlive( KeepAliveOptions( seconds(5), seconds(1), 3 ) ) );
    ASSERT_TRUE( socket->getKeepAlive( &result ) );
    ASSERT_TRUE( static_cast< bool >( result ) );

    #if defined( Q_OS_LINUX )
        EXPECT_EQ( result->inactivityPeriodBeforeFirstProbe, seconds(5) );
        EXPECT_EQ( result->probeSendPeriod, seconds(1) );
        EXPECT_EQ( result->probeCount, 3U );
    #elif defined( Q_OS_WIN )
        EXPECT_EQ( result->inactivityPeriodBeforeFirstProbe, seconds(5) );
        EXPECT_EQ( result->probeSendPeriod, seconds(1) );
        EXPECT_EQ( result->probeCount, 10U ); // means default
    #elif defined( Q_OS_MACOS )
        EXPECT_EQ( result->inactivityPeriodBeforeFirstProbe, seconds(5) );
        EXPECT_EQ( result->probeSendPeriod, seconds::zero() ); // means default
        EXPECT_EQ( result->probeCount, 0U ); // means default
    #endif

    // Disable
    ASSERT_TRUE( socket->setKeepAlive( std::nullopt ) );
    ASSERT_TRUE( socket->getKeepAlive( &result ) );
    ASSERT_FALSE( static_cast< bool >( result ) );
}

TEST_F(TcpSocket, DISABLED_KeepAliveOptionsDefaults)
{
    const auto socket = std::make_unique< TCPSocket >( AF_INET );
    std::optional< KeepAliveOptions > result;
    ASSERT_TRUE( socket->getKeepAlive( &result ) );
    ASSERT_FALSE( static_cast< bool >( result ) );
}

static void waitForKeepAliveDisconnect(AbstractStreamSocket* socket)
{
    Buffer buffer(1024, 0);
    ASSERT_TRUE(socket->setKeepAlive(KeepAliveOptions(
        std::chrono::seconds(10), std::chrono::seconds(5), 3)));

    NX_INFO(typeid(TcpSocket), nx::format("waitForKeepAliveDisconnect recv"));
    EXPECT_LT(socket->recv(buffer.data(), buffer.size()), 0);
    EXPECT_NE(SystemError::noError, SystemError::getLastOSErrorCode());
    NX_INFO(typeid(TcpSocket), nx::format("waitForKeepAliveDisconnect end"));
}

TEST_F(TcpSocket, DISABLED_KeepAliveOptionsServer)
{
    const auto server = std::make_unique<TCPServerSocket>(AF_INET);
    ASSERT_TRUE(server->setReuseAddrFlag(true));
    ASSERT_TRUE(server->bind(SocketAddress::anyAddress));
    ASSERT_TRUE(server->listen((int) testClientCount()));
    NX_INFO(this, nx::format("Server address: %1").arg(server->getLocalAddress()));

    auto client = server->accept();
    ASSERT_TRUE(client != nullptr);
    waitForKeepAliveDisconnect(client.get());
}

struct TcpSocketIpv4Factory
{
    std::unique_ptr<nx::network::TCPSocket> operator()() const
    {
        return std::make_unique<nx::network::TCPSocket>(AF_INET);
    }
};

INSTANTIATE_TYPED_TEST_SUITE_P(TcpSocket, SocketOptions, TcpSocketIpv4Factory);
INSTANTIATE_TYPED_TEST_SUITE_P(TcpSocket, SocketOptionsDefaultValue, TcpSocketIpv4Factory);

struct TcpSocketIpv6Factory
{
    std::unique_ptr<nx::network::TCPSocket> operator()() const
    {
        return std::make_unique<nx::network::TCPSocket>(AF_INET6);
    }
};

INSTANTIATE_TYPED_TEST_SUITE_P(TcpSocket6, SocketOptions, TcpSocketIpv6Factory);
INSTANTIATE_TYPED_TEST_SUITE_P(TcpSocket6, SocketOptionsDefaultValue, TcpSocketIpv6Factory);

TEST_F(TcpSocket, socket_timer_is_single_shot)
{
    constexpr auto kTimeout = std::chrono::milliseconds(10);

    std::atomic<int> triggerCount(0);

    TCPSocket tcpSocket(AF_INET);
    tcpSocket.registerTimer(kTimeout, [&triggerCount]() { ++triggerCount; });

    std::this_thread::sleep_for(std::chrono::seconds(1));
    tcpSocket.pleaseStopSync();

    ASSERT_EQ(1, triggerCount.load());
}

TEST_F(TcpSocket, ConnectErrorReporting)
{
    // Connecting to port 1 which is reserved for now deprecated TCPMUX service.
    // So, in most cases can expect port 1 to be unused.
    const SocketAddress localAddress(HostAddress::localhost, 1);

    TCPSocket tcpSocket(AF_INET);
    const bool connectResult = tcpSocket.connect(localAddress, std::chrono::milliseconds(100));
    const auto connectErrorCode = SystemError::getLastOSErrorCode();
    ASSERT_FALSE(connectResult);
    ASSERT_NE(SystemError::noError, connectErrorCode);
    ASSERT_FALSE(tcpSocket.isConnected());
}

TEST_F(TcpSocket, accepted_socket_timeouts_properly_reported)
{
    whenAcceptedSocket();
    thenTimeoutsAreReportedCorrectly();
}

#ifndef _WIN32

// Child process spawned while a connection is open must not keep the connection alive after the
// server has closed it. The accepted descriptor must be marked close-on-exec for that.
TEST_F(TcpSocket, accepted_socket_is_not_inherited_by_child_process)
{
    whenAcceptedSocket();
    thenAcceptedSocketHasCloseOnExecFlag();

    whenSpawnLongLivingChildProcess();
    whenCloseAcceptedSocket();

    thenClientReceivesEofImmediately();
}

#endif

#ifndef _WIN32

namespace {

// Allocations made on this thread while s_allocateInPages is set get a page of their own, unmapped
// on delete, so that a read of freed memory traps instead of returning stale data.
thread_local bool s_allocateInPages = false;
std::atomic<int> s_pageAllocationCount{0};
std::mutex s_pageAllocationsMutex;
std::array<std::pair<void*, std::size_t>, 64> s_pageAllocations;

void* allocateInPages(std::size_t size)
{
    const auto pageSize = (std::size_t) sysconf(_SC_PAGESIZE);
    const auto mapSize = (size + pageSize - 1) / pageSize * pageSize;
    void* p = mmap(nullptr, mapSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED)
        throw std::bad_alloc();

    std::lock_guard lock(s_pageAllocationsMutex);
    for (auto& slot: s_pageAllocations)
    {
        if (slot.first)
            continue;
        slot = {p, mapSize};
        ++s_pageAllocationCount;
        return p;
    }
    munmap(p, mapSize);
    throw std::bad_alloc();
}

bool unmapIfAllocatedInPages(void* ptr)
{
    if (s_pageAllocationCount.load() == 0)
        return false;

    std::lock_guard lock(s_pageAllocationsMutex);
    for (auto& [p, size]: s_pageAllocations)
    {
        if (p != ptr)
            continue;
        munmap(p, size);
        p = nullptr;
        --s_pageAllocationCount;
        return true;
    }
    return false;
}

} // namespace

} // namespace nx::network::test

void* operator new(std::size_t size)
{
    if (nx::network::test::s_allocateInPages)
        return nx::network::test::allocateInPages(size);
    if (void* p = std::malloc(size))
        return p;
    throw std::bad_alloc();
}

void operator delete(void* ptr) noexcept
{
    if (!nx::network::test::unmapIfAllocatedInPages(ptr))
        std::free(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept
{
    operator delete(ptr);
}

namespace nx::network::test {

TEST_F(TcpSocket, deleted_from_its_own_connect_handler)
{
    // Every object the socket constructor allocates, the AIO helper among them, gets its own page,
    // unmapped when freed: a read of the helper after the handler has deleted the socket traps.
    s_allocateInPages = true;
    auto socket = new TCPSocket(AF_INET);
    s_allocateInPages = false;
    ASSERT_GT(s_pageAllocationCount.load(), 0);
    ASSERT_TRUE(socket->setNonBlockingMode(true));
    auto aioThread = socket->getAioThread();

    std::promise<SystemError::ErrorCode> connected;
    auto connectedFuture = connected.get_future();
    socket->connectAsync(serverAddress(),
        [socket, connected = std::move(connected)](SystemError::ErrorCode code) mutable
        {
            delete socket;
            connected.set_value(code);
        });
    ASSERT_EQ(SystemError::noError, connectedFuture.get());

    // What runs after the handler returns is what reads freed memory; let the thread get there.
    std::promise<void> drained;
    auto drainedFuture = drained.get_future();
    aioThread->post(nullptr, [drained = std::move(drained)]() mutable { drained.set_value(); });
    drainedFuture.wait();
    ASSERT_EQ(0, s_pageAllocationCount.load());
}

#endif // !_WIN32

TEST(TcpServerSocketIpv6, BindsToLocalAddress)
{
    TCPServerSocket socket(AF_INET6);
    ASSERT_TRUE(socket.bind(SocketAddress::anyPrivateAddress));
    ASSERT_EQ(
        HostAddress::localhost.toPureIpAddress(AF_INET6).toString(),
        socket.getLocalAddress().address.toString());
}

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST, TcpSocketV4,
    [](){ return std::make_unique<TCPServerSocket>(AF_INET); },
    [](){ return std::make_unique<TCPSocket>(AF_INET); })

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST, TcpSocketV6,
    [](){ return std::make_unique<TCPServerSocket>(AF_INET6); },
    [](){ return std::make_unique<TCPSocket>(AF_INET6); })

// NOTE: Currently IP v4 address 127.0.0.1 is represented as ::1 IP v6 address, which is not true.
// TODO: Enable these tests when IP v6 is properly supported.
NX_NETWORK_TRANSFER_SOCKET_TESTS_CASE(
    TEST, DISABLED_TcpSocketV4to6,
    [](){ return std::make_unique<TCPServerSocket>(AF_INET6); },
    [](){ return std::make_unique<TCPSocket>(AF_INET); })

NX_NETWORK_TRANSFER_SOCKET_TESTS_CASE(
    TEST, DISABLED_TcpSocketV6to4,
    [](){ return std::make_unique<TCPServerSocket>(AF_INET); },
    [](){ return std::make_unique<TCPSocket>(AF_INET6); })

} // nx::network::test
