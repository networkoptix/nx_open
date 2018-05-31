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

#include <managers/time_manager.h>
#include <rest/handlers/time_sync_rest_handler.h>
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

class TimeSynchronizationPeer
{
public:
    TimeSynchronizationPeer():
        m_commonModule(
            /*clientMode*/ false,
            nx::core::access::Mode::direct),
        m_testSystemClock(std::make_shared<TestSystemClock>()),
        m_testSteadyClock(std::make_shared<TestSteadyClock>()),
        m_miscManager(std::make_shared<MiscManagerStub>()),
        m_transactionMessageBus(std::make_unique<TransactionMessageBusStub>(&m_commonModule)),
        m_workAroundMiscDataSaverStub(
            std::make_shared<WorkAroundMiscDataSaverStub>(m_miscManager.get())),
        m_timeSynchronizationManager(std::make_unique<TimeSynchronizationManager>(
            commonModule(),
            Qn::PeerType::PT_Server,
            &m_timerManager,
            &m_settings,
            m_workAroundMiscDataSaverStub,
            m_testSystemClock,
            m_testSteadyClock))
    {
        m_commonModule.setModuleGUID(QnUuid::createUuid());

        m_commonModule.globalSettings()->setOsTimeChangeCheckPeriod(
            std::chrono::milliseconds(100));
        m_commonModule.globalSettings()->setSyncTimeExchangePeriod(
            std::chrono::seconds(1));

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

    bool start()
    {
        if (!startHttpServer())
            return false;
        m_timeSynchronizationManager->start(m_transactionMessageBus.get(), m_miscManager);
        return true;
    }

    std::chrono::milliseconds getSyncTime() const
    {
        return std::chrono::milliseconds(m_timeSynchronizationManager->getSyncTime());
    }

    void stop()
    {
        m_timeSynchronizationManager.reset();
        m_transactionMessageBus.reset();
    }

    void restart()
    {
        if (m_timeSynchronizationManager)
            m_timeSynchronizationManager.reset();
        if (m_transactionMessageBus)
            m_transactionMessageBus.reset();

        m_transactionMessageBus =
            std::make_unique<TransactionMessageBusStub>(&m_commonModule);
        m_timeSynchronizationManager = std::make_unique<TimeSynchronizationManager>(
            commonModule(),
            Qn::PeerType::PT_Server,
            &m_timerManager,
            &m_settings,
            m_workAroundMiscDataSaverStub,
            m_testSystemClock,
            m_testSteadyClock);
        m_timeSynchronizationManager->start(m_transactionMessageBus.get(), m_miscManager);
    }

    void setPrimaryPeerId(const QnUuid& peerId)
    {
        m_timeSynchronizationManager->primaryTimeServerChanged(nx::vms::api::IdData(peerId));
    }

    void connectTo(TimeSynchronizationPeer* remotePeer)
    {
        m_transactionMessageBus->addConnectionToRemotePeer(
            peerData(),
            remotePeer->peerData(),
            remotePeer->apiUrl(),
            /*isIncoming*/ false);

        remotePeer->emulatePeerConnectionAccept(this);
    }

    void emulatePeerConnectionAccept(TimeSynchronizationPeer* remotePeer)
    {
        m_transactionMessageBus->addConnectionToRemotePeer(
            peerData(),
            remotePeer->peerData(),
            remotePeer->apiUrl(),
            /*isIncoming*/ true);
    }

    ::ec2::ApiPeerData peerData() const
    {
        ::ec2::ApiPeerData peerData;
        peerData.id = m_commonModule.moduleGUID();
        peerData.peerType = Qn::PT_Server;
        return peerData;
    }

    nx::utils::Url apiUrl() const
    {
        return nx::network::url::Builder()
            .setScheme(nx::network::http::kUrlSchemeName)
            .setEndpoint(m_httpServer.serverAddress()).toUrl();
    }

    std::chrono::milliseconds getSystemTime() const
    {
        return m_testSystemClock->millisSinceEpoch();
    }

    QnGlobalSettings* globalSettings() const
    {
        return m_commonModule.globalSettings();
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
    nx::utils::StandaloneTimerManager m_timerManager;
    QnCommonModule m_commonModule;
    Settings m_settings;
    nx::network::http::TestHttpServer m_httpServer;
    std::shared_ptr<TestSystemClock> m_testSystemClock;
    std::shared_ptr<TestSteadyClock> m_testSteadyClock;
    std::shared_ptr<MiscManagerStub> m_miscManager;
    std::shared_ptr<AbstractECConnection> m_connection;
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

    bool startHttpServer()
    {
        using namespace std::placeholders;

        if (!m_httpServer.bindAndListen())
            return false;

        m_httpServer.registerRequestProcessorFunc(
            nx::network::url::joinPath("/", TimeSynchronizationManager::kTimeSyncUrlPath.toStdString()).c_str(),
            std::bind(&TimeSynchronizationPeer::syncTimeHttpHandler, this, _1, _2, _3, _4, _5));

        return true;
    }

    void syncTimeHttpHandler(
        nx::network::http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx::network::http::Request request,
        nx::network::http::Response* const response,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        const auto resultCode = QnTimeSyncRestHandler::processRequest(
            request,
            m_timeSynchronizationManager.get(),
            connection->socket().get());
        if (resultCode != nx::network::http::StatusCode::ok)
            return completionHandler(resultCode);

        // Sending our time synchronization information to remote peer.
        QnTimeSyncRestHandler::prepareResponse(
            *m_timeSynchronizationManager,
            m_commonModule,
            response);

        return completionHandler(nx::network::http::StatusCode::ok);
    }
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
} // namespace ec2
