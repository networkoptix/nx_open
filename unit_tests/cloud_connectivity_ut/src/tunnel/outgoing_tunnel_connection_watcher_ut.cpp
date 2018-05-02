#include <gtest/gtest.h>

#include <nx/network/aio/aio_service.h>
#include <nx/network/cloud/tunnel/outgoing_tunnel_connection_watcher.h>
#include <nx/network/socket_global.h>
#include <nx/utils/atomic_unique_ptr.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>

#include <nx/utils/scope_guard.h>

#include "tunnel_connection_stub.h"

namespace nx {
namespace network {
namespace cloud {
namespace test {

namespace {

constexpr auto tunnelInactivityTimeout = std::chrono::seconds(3);
constexpr auto allowedTimerError = std::chrono::seconds(5);

class OutgoingTunnelConnectionWatcher:
    public ::testing::Test
{
public:
    typedef ScopeGuard<std::function<void()>> InitializationGuard;

    OutgoingTunnelConnectionWatcher():
        m_tunnelAioThread(nullptr),
        m_tunnelCloseEventAioThread(nullptr)
    {
        m_connectionParameters.tunnelInactivityTimeout =
            tunnelInactivityTimeout;
    }

protected:
    InitializationGuard initializeTunnel()
    {
        m_tunnel = std::make_unique<cloud::OutgoingTunnelConnectionWatcher>(
            m_connectionParameters,
            std::make_unique<TunnelConnectionStub>());

        m_tunnelAioThread = network::SocketGlobals::aioService().getRandomAioThread();
        m_tunnel->bindToAioThread(m_tunnelAioThread);

        m_tunnel->setControlConnectionClosedHandler(
            [this](SystemError::ErrorCode reason)
            {
                m_tunnel.reset();
                m_tunnelCloseEventAioThread =
                    network::SocketGlobals::aioService().getCurrentAioThread();
                m_tunnelClosedPromise.set_value(reason);
            });

        m_tunnel->start();

        return InitializationGuard(
            [this]()
            {
                decltype(m_tunnel) tunnel(m_tunnel.release());
                if (tunnel)
                    tunnel->pleaseStopSync();
            });
    }

    void waitForTunnelToExpire(
        nx::utils::future<SystemError::ErrorCode>& tunnelClosedFuture)
    {
        ASSERT_EQ(
            std::future_status::ready,
            tunnelClosedFuture.wait_for(tunnelInactivityTimeout + allowedTimerError));
        ASSERT_EQ(SystemError::timedOut, tunnelClosedFuture.get());
    }

    void givenActiveTunnel()
    {
        m_connectionParameters.tunnelInactivityTimeout = std::chrono::seconds(1);
        m_initializationGuard = initializeTunnel();
    }

    void whenTunnelHasBeenClosedByTimeout()
    {
        ASSERT_EQ(SystemError::timedOut, m_tunnelClosedPromise.get_future().get());
    }

    void thenClosureEventShouldBeReportedWithinTunnelsAioThread()
    {
        ASSERT_EQ(m_tunnelCloseEventAioThread, m_tunnelAioThread);
    }

    nx::hpm::api::ConnectionParameters m_connectionParameters;
    nx::utils::AtomicUniquePtr<cloud::OutgoingTunnelConnectionWatcher> m_tunnel;
    nx::utils::promise<SystemError::ErrorCode> m_tunnelClosedPromise;
    InitializationGuard m_initializationGuard;
    aio::AbstractAioThread* m_tunnelAioThread;
    aio::AbstractAioThread* m_tunnelCloseEventAioThread;
};

} // namespace

TEST_F(OutgoingTunnelConnectionWatcher, unused_tunnel)
{
    const auto initializationGuard = initializeTunnel();

    auto tunnelClosedFuture = m_tunnelClosedPromise.get_future();
    waitForTunnelToExpire(tunnelClosedFuture);
}

TEST_F(OutgoingTunnelConnectionWatcher, using_tunnel)
{
    const auto initializationGuard = initializeTunnel();

    auto tunnelClosedFuture = m_tunnelClosedPromise.get_future();

    const auto deadline =
        std::chrono::steady_clock::now() +
        tunnelInactivityTimeout + allowedTimerError;

    while (std::chrono::steady_clock::now() < deadline)
    {
        m_tunnel->establishNewConnection(
            std::chrono::milliseconds::zero(),
            SocketAttributes(),
            [](SystemError::ErrorCode,
               std::unique_ptr<nx::network::AbstractStreamSocket>,
               bool /*stillValid*/)
            {
            });

        std::this_thread::sleep_for((deadline - std::chrono::steady_clock::now()) / 10);
        ASSERT_EQ(
            std::future_status::timeout,
            tunnelClosedFuture.wait_for(std::chrono::seconds::zero()));
    }

    waitForTunnelToExpire(tunnelClosedFuture);
}

TEST_F(OutgoingTunnelConnectionWatcher, tunnel_closure_by_timeout_reported_from_correct_aio_thread)
{
    givenActiveTunnel();
    whenTunnelHasBeenClosedByTimeout();
    thenClosureEventShouldBeReportedWithinTunnelsAioThread();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
