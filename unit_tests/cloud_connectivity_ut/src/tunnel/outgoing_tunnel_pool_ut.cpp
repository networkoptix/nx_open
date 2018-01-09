#include <deque>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/outgoing_tunnel.h>
#include <nx/network/cloud/tunnel/outgoing_tunnel_pool.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

namespace {

class OutgoingTunnelStub:
    public AbstractOutgoingTunnel
{
public:
    OutgoingTunnelStub(const AddressEntry& targetHost):
        m_targetHost(targetHost)
    {
    }

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override
    {
        m_pleaseStopHandler = std::move(handler);
    }

    virtual void setOnClosedHandler(nx::utils::MoveOnlyFunc<void()> /*handler*/) override
    {
    }

    virtual void establishNewConnection(
        std::chrono::milliseconds /*timeout*/,
        SocketAttributes /*socketAttributes*/,
        NewConnectionHandler handler) override
    {
        post(
            [this, handler = std::move(handler)]() mutable
            {
                m_connectHandlers.push_back(std::move(handler));
            });
    }

    void completeOngoingOperations(SystemError::ErrorCode systemErrorCode)
    {
        post(
            [this, systemErrorCode]()
            {
                decltype(m_connectHandlers) connectHandlers;
                connectHandlers.swap(m_connectHandlers);
                for (auto& handler: connectHandlers)
                    handler(systemErrorCode, TunnelAttributes(), nullptr);
            });
    }

    void completeStopping()
    {
        post([this]() { nx::utils::swapAndCall(m_pleaseStopHandler); });
    }

private:
    AddressEntry m_targetHost;
    std::deque<NewConnectionHandler> m_connectHandlers;
    nx::utils::MoveOnlyFunc<void()> m_pleaseStopHandler;
};

} // namespace

//-------------------------------------------------------------------------------------------------

class OutgoingTunnelPool:
    public ::testing::Test
{
public:
    OutgoingTunnelPool()
    {
        using namespace std::placeholders;

        m_factoryBak = OutgoingTunnelFactory::instance().setCustomFunc(
            std::bind(&OutgoingTunnelPool::createOutgoingTunnel, this, _1));
    }

    ~OutgoingTunnelPool()
    {
        OutgoingTunnelFactory::instance().setCustomFunc(std::move(m_factoryBak));
    }

protected:
    void whenRequestConnection()
    {
        using namespace std::placeholders;

        m_tunnelPool.establishNewConnection(
            AddressEntry("ty.ru"),
            std::chrono::milliseconds::zero(),
            SocketAttributes(),
            std::bind(&OutgoingTunnelPool::saveConnectResult, this, _1, _2, _3));
    }

    void whenInitiateTunnelPoolStop()
    {
        m_tunnelPool.pleaseStop(std::bind(&OutgoingTunnelPool::tunnelPoolStopped, this));
    }

    void whenTunnelInvokesCompletionHandler()
    {
        m_lastCreatedTunnel->completeOngoingOperations(SystemError::connectionReset);
    }

    void whenTunnelStopped()
    {
        m_lastCreatedTunnel->completeStopping();
    }

    void thenTunnelPoolStopped()
    {
        m_tunnelStopped.get_future().wait();
    }

private:
    OutgoingTunnelFactory::Function m_factoryBak;
    cloud::OutgoingTunnelPool m_tunnelPool;
    OutgoingTunnelStub* m_lastCreatedTunnel = nullptr;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_connectResults;
    nx::utils::promise<void> m_tunnelStopped;

    std::unique_ptr<AbstractOutgoingTunnel> createOutgoingTunnel(
        const AddressEntry& targetHost)
    {
        auto tunnel = std::make_unique<OutgoingTunnelStub>(targetHost);
        m_lastCreatedTunnel = tunnel.get();
        return std::move(tunnel);
    }

    void saveConnectResult(
        SystemError::ErrorCode systemErrorCode,
        TunnelAttributes /*tunnelAttributes*/,
        std::unique_ptr<nx::network::AbstractStreamSocket> /*connection*/)
    {
        m_connectResults.push(systemErrorCode);
    }

    void tunnelPoolStopped()
    {
        m_tunnelStopped.set_value();
    }
};

TEST_F(OutgoingTunnelPool, tunnel_reports_completion_after_pleaseStop_invoked)
{
    whenRequestConnection();
    whenInitiateTunnelPoolStop();
    whenTunnelInvokesCompletionHandler();
    whenTunnelStopped();

    thenTunnelPoolStopped();
    // Then application does not crash.
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
