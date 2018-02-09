#include <limits>

#include <gtest/gtest.h>

#include <nx/utils/timer_manager.h>
#include <nx/utils/time.h>

#include <nx/core/access/access_types.h>

#include <api/global_settings.h>
#include <common/common_module.h>

#include <managers/time_manager.h>
#include <settings.h>

#include "misc_manager_stub.h"
#include "transaction_message_bus_stub.h"

namespace ec2 {
namespace test {

TEST(TimePriorityKey, common)
{
    ec2::TimePriorityKey key1;
    key1.sequence = 1;
    ec2::TimePriorityKey key2;
    key2.sequence = 2;

    ASSERT_TRUE(key1.hasLessPriorityThan(key2, true));
    ASSERT_FALSE(key2.hasLessPriorityThan(key1, true));

    key2.sequence = std::numeric_limits<decltype(ec2::TimePriorityKey::sequence)>::max()-1;
    ASSERT_FALSE(key1.hasLessPriorityThan(key2, true));
    ASSERT_TRUE(key2.hasLessPriorityThan(key1, true));
}

//-------------------------------------------------------------------------------------------------

namespace {

class WorkAroundMiscDataSaverStub:
    public AbstractWorkAroundMiscDataSaver
{
public:
    virtual ErrorCode saveSync(const ApiMiscData& /*data*/) override
    {
        // TODO
        return ErrorCode::ok;
    }
};

class TimeSynchronizationPeer
{
public:
    TimeSynchronizationPeer():
        m_commonModule(
            /*clientMode*/ false,
            nx::core::access::Mode::direct),
        m_transactionMessageBus(&m_commonModule),
        m_timeSynchronizationManager(
            Qn::PeerType::PT_Server,
            &m_timerManager,
            &m_transactionMessageBus,
            &m_settings,
            std::make_shared<WorkAroundMiscDataSaverStub>())
    {
        m_commonModule.setModuleGUID(QnUuid::createUuid());
        m_timerManager.start();
    }

    ~TimeSynchronizationPeer()
    {
        m_timerManager.stop();
    }

    void setSynchronizeWithInternetEnabled(bool val)
    {
        m_commonModule.globalSettings()->setSynchronizingTimeWithInternet(val);
    }

    void start()
    {
        m_timeSynchronizationManager.start(m_miscManager);
    }

    std::chrono::milliseconds getSyncTime() const
    {
        return std::chrono::milliseconds(m_timeSynchronizationManager.getSyncTime());
    }

private:
    nx::utils::StandaloneTimerManager m_timerManager;
    QnCommonModule m_commonModule;
    TransactionMessageBusStub m_transactionMessageBus;
    Settings m_settings;
    std::shared_ptr<MiscManagerStub> m_miscManager;
    TimeSynchronizationManager m_timeSynchronizationManager;
};

} // namespace

class TimeManager:
    public ::testing::Test
{
protected:
    void givenSingleOfflinePeer()
    {
        auto peer = std::make_unique<TimeSynchronizationPeer>();
        peer->setSynchronizeWithInternetEnabled(false);
        peer->start();
        m_peers.push_back(std::move(peer));
    }

    void givenMultipleOfflinePeersEachWithDifferentLocalTime()
    {
        // TODO
    }

    void shiftLocalTime()
    {
        m_timeShifts.push_back(
            nx::utils::test::ScopedTimeShift(
                nx::utils::test::ClockType::system,
                -std::chrono::hours(1)));
    }

    void waitForTimeToBeSynchronizedAccrossAllPeers()
    {
        // TODO
    }

    void waitForTimeToBeSynchronizedWithLocalSystemClock()
    {
        for (;;)
        {
            if (m_peers.front()->getSyncTime() == nx::utils::millisSinceEpoch())
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

private:
    std::vector<std::unique_ptr<TimeSynchronizationPeer>> m_peers;
    std::vector<nx::utils::test::ScopedTimeShift> m_timeShifts;
};

TEST_F(TimeManager, single_server_synchronizes_time_with_local_system_clock)
{
    givenSingleOfflinePeer();
    waitForTimeToBeSynchronizedWithLocalSystemClock();

    shiftLocalTime();
    waitForTimeToBeSynchronizedWithLocalSystemClock();
}

TEST_F(TimeManager, DISABLED_time_is_synchronized)
{
    givenMultipleOfflinePeersEachWithDifferentLocalTime();
    waitForTimeToBeSynchronizedAccrossAllPeers();
}

} // namespace test
} // namespace ec2
