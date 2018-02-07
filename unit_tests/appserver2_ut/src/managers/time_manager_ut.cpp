#include <cmath>
#include <cstdlib>
#include <limits>

#include <gtest/gtest.h>

#include <nx/utils/timer_manager.h>
#include <nx/utils/time.h>

#include <nx/core/access/access_types.h>

#include <api/global_settings.h>
#include <api/runtime_info_manager.h>
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

class TimeSynchronizationPeer
{
public:
    TimeSynchronizationPeer():
        m_commonModule(
            /*clientMode*/ false,
            nx::core::access::Mode::direct),
        m_miscManager(std::make_shared<MiscManagerStub>()),
        m_transactionMessageBus(std::make_unique<TransactionMessageBusStub>(&m_commonModule)),
        m_workAroundMiscDataSaverStub(
            std::make_shared<WorkAroundMiscDataSaverStub>(m_miscManager.get())),
        m_timeSynchronizationManager(std::make_unique<TimeSynchronizationManager>(
            Qn::PeerType::PT_Server,
            &m_timerManager,
            m_transactionMessageBus.get(),
            &m_settings,
            m_workAroundMiscDataSaverStub))
    {
        m_commonModule.setModuleGUID(QnUuid::createUuid());
        m_commonModule.globalSettings()->setOsTimeChangeCheckPeriod(
            std::chrono::milliseconds(100));
        initializeRuntimeInfo();

        m_eventLoopThread.start();
        m_workAroundMiscDataSaverStub->moveToThread(&m_eventLoopThread);

        m_timerManager.start();
    }

    ~TimeSynchronizationPeer()
    {
        m_timeSynchronizationManager.reset();

        m_eventLoopThread.exit(0);
        m_eventLoopThread.wait();
    }

    QnCommonModule* commonModule()
    {
        return &m_commonModule;
    }

    void setSynchronizeWithInternetEnabled(bool val)
    {
        m_commonModule.globalSettings()->setSynchronizingTimeWithInternet(val);
    }

    void start()
    {
        m_timeSynchronizationManager->start(m_miscManager);
    }

    std::chrono::milliseconds getSyncTime() const
    {
        return std::chrono::milliseconds(m_timeSynchronizationManager->getSyncTime());
    }

    void emulateRestart()
    {
        m_timeSynchronizationManager.reset();
        m_transactionMessageBus.reset();

        m_transactionMessageBus =
            std::make_unique<TransactionMessageBusStub>(&m_commonModule);
        m_timeSynchronizationManager = std::make_unique<TimeSynchronizationManager>(
            Qn::PeerType::PT_Server,
            &m_timerManager,
            m_transactionMessageBus.get(),
            &m_settings,
            m_workAroundMiscDataSaverStub);
        m_timeSynchronizationManager->start(m_miscManager);
    }

private:
    nx::utils::StandaloneTimerManager m_timerManager;
    QnCommonModule m_commonModule;
    Settings m_settings;
    std::shared_ptr<MiscManagerStub> m_miscManager;
    std::unique_ptr<TransactionMessageBusStub> m_transactionMessageBus;
    std::shared_ptr<WorkAroundMiscDataSaverStub> m_workAroundMiscDataSaverStub;
    std::unique_ptr<TimeSynchronizationManager> m_timeSynchronizationManager;
    QThread m_eventLoopThread;

    void initializeRuntimeInfo()
    {
        QnPeerRuntimeInfo localPeerInfo;
        localPeerInfo.uuid = m_commonModule.moduleGUID();
        localPeerInfo.data.peer.peerType = Qn::PT_Server;
        localPeerInfo.data.peer.id = m_commonModule.moduleGUID();
        localPeerInfo.data.peer.instanceId = m_commonModule.moduleGUID();
        localPeerInfo.data.peer.persistentId = m_commonModule.moduleGUID();
        m_commonModule.runtimeInfoManager()->updateLocalItem(localPeerInfo);
    }
};

} // namespace

//-------------------------------------------------------------------------------------------------

class TimeManager:
    public ::testing::Test
{
public:
    TimeManager():
        m_timeShift(
            nx::utils::test::ClockType::system,
            std::chrono::seconds::zero())
    {
    }

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
        m_timeShift.applyRelativeShift(-std::chrono::hours(1));
    }

    void restartPeer()
    {
        m_peers.front()->emulateRestart();
    }

    void waitForTimeToBeSynchronizedAccrossAllPeers()
    {
        // TODO
    }

    void waitForTimeToBeSynchronizedWithLocalSystemClock()
    {
        for (;;)
        {
            const auto syncTime = m_peers.front()->getSyncTime();
            const auto osTime = nx::utils::millisSinceEpoch();
            if (std::abs((syncTime - osTime).count()) <=
                m_peers.front()->commonModule()->globalSettings()
                    ->maxDifferenceBetweenSynchronizedAndLocalTime().count())
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

private:
    std::vector<std::unique_ptr<TimeSynchronizationPeer>> m_peers;
    nx::utils::test::ScopedTimeShift m_timeShift;
};

//-------------------------------------------------------------------------------------------------

TEST_F(TimeManager, single_offline_server_follows_local_system_clock)
{
    givenSingleOfflinePeer();
    waitForTimeToBeSynchronizedWithLocalSystemClock();

    shiftLocalTime();
    waitForTimeToBeSynchronizedWithLocalSystemClock();

    restartPeer();
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
