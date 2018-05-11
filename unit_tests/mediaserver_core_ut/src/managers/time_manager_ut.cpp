#include <cmath>
#include <cstdlib>
#include <limits>

#include <gtest/gtest.h>

#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/random.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/time.h>

#include <nx/core/access/access_types.h>

#include <api/global_settings.h>
#include <api/http_client_pool.h>
#include <api/runtime_info_manager.h>
#include <common/common_module.h>

#include <nx/time_sync/server_time_sync_manager.h>
#include <settings.h>
#include <api/model/time_reply.h>

#include "misc_manager_stub.h"
#include "transaction_message_bus_stub.h"
#include <rest/server/json_rest_result.h>
#include <nx_ec/ec_api.h>
#include <managers/misc_manager_stub.h>
#include <nx/utils/test_support/module_instance_launcher.h>
#include <test_support/appserver2_process.h>
#include <nx/utils/test_support/test_options.h>
#include <transaction/message_bus_adapter.h>

using namespace ec2::test;

using Appserver2 = nx::utils::test::ModuleLauncher<::ec2::Appserver2ProcessPublic>;
using Appserver2Ptr = std::unique_ptr<Appserver2>;

namespace nx {
namespace time_sync {
namespace test {

namespace {

class TestSystemClock:
    public AbstractSystemClock
{
public:
    virtual std::chrono::milliseconds millisSinceEpoch() override
    {
        return nx::utils::millisSinceEpoch() + m_timeShift;
    }

    void applyRelativeShift(std::chrono::milliseconds shift)
    {
        m_timeShift += shift;
    }

private:
    std::chrono::milliseconds m_timeShift{0};
};

//-------------------------------------------------------------------------------------------------

class TestSteadyClock:
    public AbstractSteadyClock
{
public:
    virtual std::chrono::milliseconds now() override
    {
        return m_steadyClock.now() + m_timeShift;
    }

    void applyRelativeShift(std::chrono::milliseconds shift)
    {
        m_timeShift += shift;
    }

private:
    SteadyClock m_steadyClock;
    std::chrono::milliseconds m_timeShift{ 0 };
};

//-------------------------------------------------------------------------------------------------

static Appserver2Ptr createAppserver()
{
    static int instanceCounter = 0;
    const auto tmpDir = nx::utils::TestOptions::temporaryDirectoryPath() +
        lit("/ec2_server_sync_ut.data") + QString::number(instanceCounter++);
    QDir(tmpDir).removeRecursively();

    Appserver2Ptr result(new Appserver2());

    const QString dbFileArg = lit("--dbFile=%1").arg(tmpDir);
    result->addArg(dbFileArg.toStdString().c_str());

    result->start();
    result->waitUntilStarted();
    return result;
}

class TimeSynchronizationPeer
{
public:
    TimeSynchronizationPeer()
    {
    }

    ~TimeSynchronizationPeer()
    {
        m_appserver.reset();
    }

    QnCommonModule* commonModule() const
    {
        return m_appserver->moduleInstance()->commonModule();
    }

    void setSynchronizeWithInternetEnabled(bool val)
    {
        commonModule()->globalSettings()->setSynchronizingTimeWithInternet(val);
    }

    bool start()
    {
        m_appserver = createAppserver();

        m_testSystemClock = std::make_shared<TestSystemClock>();
        m_testSteadyClock = std::make_shared<TestSteadyClock>();
        auto timeSyncManager = m_appserver->moduleInstance()->ecConnection()->timeSyncManager();
        timeSyncManager->setClock(m_testSystemClock, m_testSteadyClock);

        auto commonModule = m_appserver->moduleInstance()->commonModule();
        commonModule->globalSettings()->setOsTimeChangeCheckPeriod(
            std::chrono::milliseconds(100));
        commonModule->globalSettings()->setSyncTimeExchangePeriod(
            std::chrono::seconds(1));

        return true;
    }

    std::chrono::milliseconds getSyncTime() const
    {
        auto timeSyncManager = commonModule()->ec2Connection()->timeSyncManager();
        return std::chrono::milliseconds(timeSyncManager->getSyncTime());
    }

    void stop()
    {
        m_appserver.reset();
    }

    void restart()
    {
        stop();
        start();
    }

    void setPrimaryPeerId(const QnUuid& peerId)
    {
        commonModule()->globalSettings()->setPrimaryTimeServer(peerId);
    }

    void connectTo(TimeSynchronizationPeer* remotePeer)
    {
        const auto id = remotePeer->peerData().id;
        auto messageBus = m_appserver->moduleInstance()->ecConnection()->messageBus();
        messageBus->addOutgoingConnectionToPeer(id, remotePeer->apiUrl());
    }

    ::ec2::ApiPeerData peerData() const
    {
        ::ec2::ApiPeerData peerData;
        peerData.id = commonModule()->moduleGUID();
        peerData.peerType = Qn::PT_Server;
        return peerData;
    }

    nx::utils::Url apiUrl() const
    {
        auto endpoint = m_appserver->moduleInstance()->endpoint();
        return nx::network::url::Builder()
            .setScheme(nx::network::http::kUrlSchemeName)
            .setEndpoint(endpoint).toUrl();
    }

    std::chrono::milliseconds getSystemTime() const
    {
        return m_testSystemClock->millisSinceEpoch();
    }

    QnGlobalSettings* globalSettings() const
    {
        return commonModule()->globalSettings();
    }

    void shiftSystemClock(std::chrono::milliseconds shift)
    {
        m_testSystemClock->applyRelativeShift(shift);
    }

    void skewMonotonicClock(std::chrono::milliseconds shift)
    {
        m_testSteadyClock->applyRelativeShift(shift);
    }

private:
    Appserver2Ptr m_appserver;
    std::shared_ptr<TestSystemClock> m_testSystemClock;
    std::shared_ptr<TestSteadyClock> m_testSteadyClock;
};

} // namespace

//-------------------------------------------------------------------------------------------------

// In real, time deviation depends on peer to peer request round trip time.
// Using very large value to make test reliable.
// Time difference on different peers has to be much larger than this value.
constexpr auto kMaxAllowedSyncTimeDeviation = std::chrono::seconds(21);

constexpr auto kMinMonotonicClockSkew = kMaxAllowedSyncTimeDeviation + std::chrono::seconds(10);
constexpr auto kMaxMonotonicClockSkew = kMinMonotonicClockSkew + std::chrono::minutes(2);

class TimeSynchronization:
    public ::testing::Test
{
public:
    TimeSynchronization():
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
        ASSERT_TRUE(peer->start());
        m_peers.push_back(std::move(peer));
    }

    template<int peerCount>
    void launchCluster(const int connectTable[peerCount][peerCount])
    {
        startPeers(peerCount);

        m_connectTable.resize(peerCount);
        for (int i = 0; i < peerCount; ++i)
        {
            m_connectTable[i].resize(peerCount);
            for (int j = i+1; j < peerCount; ++j)
                m_connectTable[i][j] = connectTable[i][j];
        }

        connectPeers();
    }

    void givenMultipleOfflinePeersEachWithDifferentLocalTime()
    {
        const auto peerCount = 3;
        startPeers(peerCount);

        connectEveryPeerToEachOther();
    }

    void givenMultipleSynchronizedPeers()
    {
        givenMultipleOfflinePeersEachWithDifferentLocalTime();
        waitForTimeToBeSynchronizedAcrossAllPeers();
    }

    void givenMultipleSynchronizedPeersWithPrimaryPeerSelectedByUser()
    {
        givenMultipleOfflinePeersEachWithDifferentLocalTime();
        selectRandomPeerAsPrimary();
        waitForTimeToBeSynchronizedWithPrimaryPeerSystemClock();
    }

    void selectPrimaryPeer(int peerIndex)
    {
        m_primaryPeerIndex = peerIndex;

        for (const auto& peer : m_peers)
        {
            peer->setPrimaryPeerId(
                m_peers[m_primaryPeerIndex]->commonModule()->moduleGUID());
        }
    }

    void selectRandomPeerAsPrimary()
    {
        // Selecting peer with the maximum id.
        const auto it = std::max_element(
            m_peers.begin(), m_peers.end(),
            [](const auto& left, const auto& right)
            {
                return left->commonModule()->moduleGUID() < right->commonModule()->moduleGUID();
            });
        selectPrimaryPeer(it - m_peers.begin());
    }

    void shiftLocalTime()
    {
        m_timeShift.applyRelativeShift(-std::chrono::hours(1));
    }

    void restartAllPeers()
    {
        for (const auto& peer: m_peers)
            peer->restart();

        if (m_peersConnected)
            connectEveryPeerToEachOther();
    }

    void stopAllPeers()
    {
        for (const auto& peer: m_peers)
            peer->stop();
    }

    void shiftMonotonicClockOnPeer(int peerIndex)
    {
        m_peers[peerIndex]->skewMonotonicClock(std::chrono::seconds(
            nx::utils::random::number<int>(
                kMinMonotonicClockSkew.count(), kMaxMonotonicClockSkew.count())));
    }

    void shiftMonotonicClockOnRandomPeer()
    {
        const auto randomPeerIndex =
            nx::utils::random::number<int>(0, m_peers.size() - 1);
        shiftMonotonicClockOnPeer(randomPeerIndex);
    }

    void shiftLocalTimeOnPrimaryPeer()
    {
        m_peers[m_primaryPeerIndex]->shiftSystemClock(
            std::chrono::hours(nx::utils::random::number<int>(-1000, 1000)));
    }

    void waitForTimeToBeSynchronizedAcrossAllPeers()
    {
        using namespace std::chrono;

        for (;;)
        {
            std::chrono::milliseconds syncTime;
            std::chrono::milliseconds syncTimeDeviation;
            std::tie(syncTime, syncTimeDeviation) = getClusterSyncTime();
            if (syncTimeDeviation <= kMaxAllowedSyncTimeDeviation)
            {
                std::cout << "Time synchronized with " << syncTimeDeviation.count()
                    << "ms deviation" << std::endl;
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void waitForTimeToBeSynchronizedWithPrimaryPeerSystemClock()
    {
        if (m_primaryPeerIndex == -1)
        {
            if (m_peers.size() == 1)
            {
                m_primaryPeerIndex = 0;
            }
            else
            {
                FAIL();
            }
        }

        for (;;)
        {
            std::chrono::milliseconds syncTime;
            std::chrono::milliseconds syncTimeDeviation;
            std::tie(syncTime, syncTimeDeviation) = getClusterSyncTime();
            const auto syncTimeError = syncTimeDeviation / 2;

            const auto primaryPeerSystemTime =
                m_peers[m_primaryPeerIndex]->getSystemTime();
            const auto expectedSyncPrecision =
                m_peers[m_primaryPeerIndex]->globalSettings()->
                    maxDifferenceBetweenSynchronizedAndLocalTime();

            if ((syncTimeDeviation <= kMaxAllowedSyncTimeDeviation) &&
                (syncTime + syncTimeDeviation) > (primaryPeerSystemTime - expectedSyncPrecision) &&
                (syncTime - syncTimeDeviation) < (primaryPeerSystemTime + expectedSyncPrecision))
            {
                std::cout << "Time synchronized with "
                    << std::abs(primaryPeerSystemTime.count() - syncTime.count())
                    << "ms error" << std::endl;
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

private:
    std::vector<std::unique_ptr<TimeSynchronizationPeer>> m_peers;
    nx::utils::test::ScopedTimeShift m_timeShift;
    int m_primaryPeerIndex = -1;
    bool m_peersConnected = false;
    /** m_connectTable[i][j] is true, then peer[i] connects to peer[j]. */
    std::vector<std::vector<int>> m_connectTable;
    /** Needed just to satisfy QnCommonModule requirement. */
    nx::network::http::ClientPool m_httpClientPool;

    void startPeers(int peerCount)
    {
        for (int i = 0; i < peerCount; ++i)
        {
            auto peer = std::make_unique<TimeSynchronizationPeer>();
            peer->setSynchronizeWithInternetEnabled(false);
            peer->shiftSystemClock(
                std::chrono::hours(nx::utils::random::number<int>(-1000, 1000)));
            ASSERT_TRUE(peer->start());
            m_peers.push_back(std::move(peer));
        }
    }

    void connectEveryPeerToEachOther()
    {
        m_connectTable.resize(m_peers.size());
        for (auto& val: m_connectTable)
            val.resize(m_peers.size(), 1);

        connectPeers();
    }

    void connectPeers()
    {
        for (int i = 0; i < m_peers.size(); ++i)
        {
            for (int j = i + 1; j < m_peers.size(); ++j)
            {
                if (m_connectTable[i][j])
                    m_peers[i]->connectTo(m_peers[j].get());
            }
        }

        m_peersConnected = true;
    }

    /**
     * @return (mean sync time across all peers, deviation)
     */
    std::tuple<std::chrono::milliseconds, std::chrono::milliseconds> getClusterSyncTime() const
    {
        using namespace std::chrono;

        const auto t0 = steady_clock::now();
        std::vector<milliseconds> syncTimes;
        for (const auto& peer: m_peers)
            syncTimes.push_back(peer->getSyncTime());
        const auto t1 = steady_clock::now();

        const auto maxDeviation =
            duration_cast<milliseconds>(t1 - t0) +
            milliseconds(*std::max_element(syncTimes.begin(), syncTimes.end()) -
                *std::min_element(syncTimes.begin(), syncTimes.end()));
        const auto meanSyncTime =
            std::accumulate(syncTimes.begin(), syncTimes.end(), milliseconds::zero()) /
            syncTimes.size();

        return std::make_tuple(milliseconds(meanSyncTime), maxDeviation);
    }
};

//-------------------------------------------------------------------------------------------------

TEST_F(TimeSynchronization, single_offline_server_follows_local_system_clock)
{
    givenSingleOfflinePeer();
    waitForTimeToBeSynchronizedWithPrimaryPeerSystemClock();

    shiftLocalTime();
    waitForTimeToBeSynchronizedWithPrimaryPeerSystemClock();

    restartAllPeers();
    shiftLocalTime();
    waitForTimeToBeSynchronizedWithPrimaryPeerSystemClock();
}

TEST_F(TimeSynchronization, multiple_peers_synchronize_time)
{
    givenMultipleOfflinePeersEachWithDifferentLocalTime();
    waitForTimeToBeSynchronizedAcrossAllPeers();
}

TEST_F(TimeSynchronization, multiple_peers_synchronize_time_after_monotonic_clock_skew)
{
    givenMultipleSynchronizedPeers();
    shiftMonotonicClockOnRandomPeer();
    waitForTimeToBeSynchronizedAcrossAllPeers();
}

TEST_F(TimeSynchronization, synctime_follows_selected_peer_system_clock)
{
    givenMultipleOfflinePeersEachWithDifferentLocalTime();
    selectRandomPeerAsPrimary();
    waitForTimeToBeSynchronizedWithPrimaryPeerSystemClock();

    shiftLocalTimeOnPrimaryPeer();
    waitForTimeToBeSynchronizedWithPrimaryPeerSystemClock();
}

TEST_F(TimeSynchronization, synctime_follows_selected_peer_system_clock_after_restart)
{
    givenMultipleSynchronizedPeersWithPrimaryPeerSelectedByUser();

    stopAllPeers();
    shiftLocalTimeOnPrimaryPeer();
    restartAllPeers();

    waitForTimeToBeSynchronizedWithPrimaryPeerSystemClock();
}

TEST_F(TimeSynchronization, synctime_follows_selected_peer_after_clock_skew_fix)
{
    // There are 3 peers. Peer 0 connects to peer 1. Peer 1 connects to peer 2.
    const int connectionTable[3][3] =
    {
        { 0, 1, 0 },
        { 0, 0, 1 },
        { 0, 0, 0 }
    };

    launchCluster<3>(connectionTable);
    selectPrimaryPeer(0);
    waitForTimeToBeSynchronizedWithPrimaryPeerSystemClock();

    shiftMonotonicClockOnPeer(2);
    waitForTimeToBeSynchronizedWithPrimaryPeerSystemClock();

    shiftLocalTimeOnPrimaryPeer();
    waitForTimeToBeSynchronizedWithPrimaryPeerSystemClock();
}

} // namespace test
} // namespace time_sync
} // namespace nx
