#include <condition_variable>
#include <chrono>
#include <deque>
#include <mutex>
#include <thread>

#include <gtest/gtest.h>

#include <QtCore/QElapsedTimer>
#include <QtCore/QThread>

#include <nx/network/address_resolver.h>
#include <nx/network/dns_resolver.h>
#include <nx/network/http/http_client.h>
#include <nx/network/socket_global.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/socket_test_helper.h>
#include <nx/utils/random.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/test_support/test_options.h>

#include <nx/utils/scope_guard.h>

namespace nx {
namespace network {
namespace test {

namespace { static const int kSecondsToWaitAfterTest = 5; }

static void runCancelAsyncIOTest()
{
    static const std::chrono::milliseconds kTestDuration(200);
    static const int kTestRuns = 8;
    static const int kMaxSimultaneousConnections = 100;
    static const int kBytesToSendThroughConnection = 1 * 1024;

    const QString kTestHost = QLatin1String("some-test-host-456233.com");
    std::vector<HostAddress> kTestAddresses;
    kTestAddresses.push_back(*HostAddress::ipV4from("12.34.56.78"));
    kTestAddresses.push_back(*HostAddress::ipV6from("1234::abcd").first);
    kTestAddresses.push_back(*HostAddress::ipV4from("127.0.0.1"));

    auto& dnsResolver = SocketGlobals::addressResolver().dnsResolver();
    dnsResolver.addEtcHost(kTestHost, kTestAddresses);

    auto onExit = makeScopeGuard([&]() { dnsResolver.removeEtcHost(kTestHost); });

    for (int i = 0; i < kTestRuns; ++i)
    {
        std::vector<HostAddress> kConnectAddresses;
        kConnectAddresses.push_back(HostAddress("localhost"));
        kConnectAddresses.push_back(kTestHost);
        for (const auto host : kConnectAddresses)
        {
            RandomDataTcpServer server(
                TestTrafficLimitType::none,
                kBytesToSendThroughConnection,
                SocketFactory::isSslEnforced()
                ? TestTransmissionMode::pong
                : TestTransmissionMode::spam);
            ASSERT_TRUE(server.start());

            ConnectionsGenerator connectionsGenerator(
                SocketAddress(host, server.addressBeingListened().port),
                kMaxSimultaneousConnections,
                TestTrafficLimitType::incoming,
                kBytesToSendThroughConnection,
                ConnectionsGenerator::kInfiniteConnectionCount,
                TestTransmissionMode::spam);
            connectionsGenerator.start();

            std::this_thread::sleep_for(kTestDuration);
            connectionsGenerator.pleaseStopSync();
            server.pleaseStopSync();
        }
    }

    //waiting for some calls to deleted objects
    QThread::sleep(kSecondsToWaitAfterTest);
}

TEST(Socket, AsyncOperationCancellation)
{
    runCancelAsyncIOTest();
}

TEST(SslSocket, AsyncOperationCancellation)
{
    const auto isSslEnforcedBak = SocketFactory::isSslEnforced();
    SocketFactory::enforceSsl(true);

    runCancelAsyncIOTest();

    SocketFactory::enforceSsl(isSslEnforcedBak);
}

TEST(Socket, postCancellation)
{
    constexpr static const int kTestRuns = 10;
    constexpr static const int kTestThreadCount = 10;
    constexpr static const int kTestSocketCount = 25;

    std::atomic<size_t> postCalls(0);

    auto testFunctor = [&postCalls]()
    {
        std::atomic<size_t> counter(0);

        for (int i = 0; i < kTestRuns; ++i)
        {
            size_t curCounterVal = ++counter;

            std::vector<std::unique_ptr<AbstractStreamSocket>> sockets(kTestSocketCount);
            std::for_each(
                sockets.begin(),
                sockets.end(),
                [](std::unique_ptr<AbstractStreamSocket>& ptr) { ptr = SocketFactory::createStreamSocket(); });
            //std::unique_ptr<AbstractStreamSocket> connection( SocketFactory::createStreamSocket() );

            for (const auto& sock : sockets)
                sock->post(
                    [curCounterVal, &counter, &postCalls]()
                    {
                        ASSERT_EQ(curCounterVal, (size_t)counter);
                        ++postCalls;
                        QThread::usleep(10);
                    });

            for (const auto& sock : sockets)
                sock->pleaseStopSync();

            //QThread::usleep( 100 );
        }
    };

    std::vector<nx::utils::thread> testThreads;
    for (int i = 0; i < kTestThreadCount; ++i)
        testThreads.emplace_back(nx::utils::thread(testFunctor));

    for (auto& testThread : testThreads)
        testThread.join();
}

}   //test
}   //network
}   //nx
