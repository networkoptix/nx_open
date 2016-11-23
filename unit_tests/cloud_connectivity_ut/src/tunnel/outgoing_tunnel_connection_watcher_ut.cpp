#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/outgoing_tunnel_connection_watcher.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>

#include <utils/common/guard.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

namespace {

class TestTunnelConnection:
    public AbstractOutgoingTunnelConnection
{
public:
    virtual void stopWhileInAioThread() override
    {
    }

    virtual void establishNewConnection(
        std::chrono::milliseconds /*timeout*/,
        SocketAttributes /*socketAttributes*/,
        OnNewConnectionHandler /*handler*/) override
    {
    }

    virtual void setControlConnectionClosedHandler(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> /*handler*/) override
    {
    }
};

constexpr auto tunnelInactivityTimeout = std::chrono::seconds(3);
constexpr auto allowedTimerError = std::chrono::seconds(5);

class OutgoingTunnelConnectionWatcher:
    public ::testing::Test
{
public:
    typedef ScopedGuard<std::function<void()>> InitializationGuard;

    OutgoingTunnelConnectionWatcher()
    {
        m_connectionParameters.tunnelInactivityTimeout =
            tunnelInactivityTimeout;
    }
    
    ~OutgoingTunnelConnectionWatcher()
    {
    }

protected:
    InitializationGuard initializeTunnel()
    {
        m_tunnel = std::make_unique<cloud::OutgoingTunnelConnectionWatcher>(
            m_connectionParameters,
            std::make_unique<TestTunnelConnection>());

        m_tunnel->setControlConnectionClosedHandler(
            [this](SystemError::ErrorCode reason)
            {
                m_tunnel.reset();
                m_tunnelClosedPromise.set_value(reason);
            });

        return InitializationGuard(
            [this]()
            {
                if (m_tunnel)
                    m_tunnel->pleaseStopSync();
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

    nx::hpm::api::ConnectionParameters m_connectionParameters;
    std::unique_ptr<cloud::OutgoingTunnelConnectionWatcher> m_tunnel;
    nx::utils::promise<SystemError::ErrorCode> m_tunnelClosedPromise;
};

} // namespace

TEST_F(OutgoingTunnelConnectionWatcher, unusedTunnel)
{
    const auto initializationGuard = initializeTunnel();

    auto tunnelClosedFuture = m_tunnelClosedPromise.get_future();
    waitForTunnelToExpire(tunnelClosedFuture);
}

TEST_F(OutgoingTunnelConnectionWatcher, usingTunnel)
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
               std::unique_ptr<AbstractStreamSocket>,
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

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
