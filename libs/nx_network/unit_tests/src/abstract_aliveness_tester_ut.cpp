#include <atomic>
#include <memory>

#include <gtest/gtest.h>

#include <nx/network/abstract_aliveness_tester.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::network::test {

class AlivenessTester:
    public network::AbstractAlivenessTester
{
    using base_type = network::AbstractAlivenessTester;

public:
    AlivenessTester(
        KeepAliveOptions keepAliveOptions,
        std::atomic<bool>* serverIsAlive,
        nx::utils::SyncQueue<int>* probesSent)
        :
        base_type(std::move(keepAliveOptions)),
        m_serverIsAlive(serverIsAlive),
        m_probesSent(probesSent)
    {
    }

    virtual void probe(ProbeResultHandler handler) override
    {
        m_probesSent->push(0);
        post([this, handler = std::move(handler)]() { handler(*m_serverIsAlive); });
    }

    virtual void cancelProbe() override
    {
        // TODO
    }

private:
    std::atomic<bool>* m_serverIsAlive = nullptr;
    nx::utils::SyncQueue<int>* m_probesSent = nullptr;
};

//-------------------------------------------------------------------------------------------------

class AbstractAlivenessTester:
    public ::testing::Test
{
public:
    AbstractAlivenessTester()
    {
        m_keepAliveOptions.inactivityPeriodBeforeFirstProbe = std::chrono::seconds(1);
        m_keepAliveOptions.probeSendPeriod = std::chrono::seconds(1);
        m_keepAliveOptions.probeCount = 3;
    }

    ~AbstractAlivenessTester()
    {
        if (m_alivenessTester)
            m_alivenessTester->pleaseStopSync();
    }

protected:
    void givenAliveServer()
    {
        m_serverIsAlive = true;
    }

    void givenMalfunctioningServer()
    {
        m_serverIsAlive = false;
    }

    void whenStartAlivenessTester()
    {
        m_alivenessTester = std::make_unique<AlivenessTester>(
            m_keepAliveOptions,
            &m_serverIsAlive,
            &m_probesSent);
        m_alivenessTester->start([this]() { m_failures.push(0); });
    }

    void whenWaitForMultipleSuccessfulProbes()
    {
        for (int i = 0; i < 7; ++i)
            m_probesSent.pop();
    }

    void whenStopServer()
    {
        m_serverIsAlive = false;
    }

    void thenProbeIsSended()
    {
        m_probesSent.pop();
    }

    void thenFailureIsReported()
    {
        m_failures.pop();
    }

private:
    KeepAliveOptions m_keepAliveOptions;
    std::atomic<bool> m_serverIsAlive{false};
    std::unique_ptr<AlivenessTester> m_alivenessTester;
    nx::utils::SyncQueue<int> m_failures;
    nx::utils::SyncQueue<int> m_probesSent;
};

//-------------------------------------------------------------------------------------------------

TEST_F(AbstractAlivenessTester, sends_probes)
{
    whenStartAlivenessTester();
    thenProbeIsSended();
}

TEST_F(AbstractAlivenessTester, test_failure_is_reported)
{
    givenMalfunctioningServer();
    whenStartAlivenessTester();
    thenFailureIsReported();
}

TEST_F(AbstractAlivenessTester, test_failure_is_not_reported_while_alive)
{
    givenAliveServer();

    whenStartAlivenessTester();
    whenWaitForMultipleSuccessfulProbes();
    whenStopServer();

    thenFailureIsReported();
}

//TEST_F(AbstractAlivenessTester, confirming_aliveness_prevents_unneeded_probes)

//TEST_F(AbstractAlivenessTester, can_be_restarted)

//TEST_F(AbstractAlivenessTester, can_be_reused_after_aliveness_failure)

} // namespace nx::network::test
