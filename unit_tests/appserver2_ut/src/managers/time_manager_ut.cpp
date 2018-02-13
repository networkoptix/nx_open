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
#include <api/runtime_info_manager.h>
#include <common/common_module.h>

#include <managers/time_manager.h>
#include <rest/time_sync_rest_handler.h>
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
            Qn::PeerType::PT_Server,
            &m_timerManager,
            m_transactionMessageBus.get(),
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
        m_timeSynchronizationManager->start(m_miscManager);
        return true;
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

    QUrl apiUrl() const
    {
        return nx::network::url::Builder()
            .setScheme(nx_http::kUrlSchemeName)
            .setEndpoint(m_httpServer.serverAddress()).toUrl();
    }

    void setOsTimeShift(std::chrono::milliseconds shift)
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
    TestHttpServer m_httpServer;
    std::shared_ptr<TestSystemClock> m_testSystemClock;
    std::shared_ptr<TestSteadyClock> m_testSteadyClock;
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

    bool startHttpServer()
    {
        using namespace std::placeholders;

        if (!m_httpServer.bindAndListen())
            return false;

        m_httpServer.registerRequestProcessorFunc(
            nx::network::url::joinPath("/", QnTimeSyncRestHandler::PATH),
            std::bind(&TimeSynchronizationPeer::syncTimeHttpHandler, this, _1, _2, _3, _4, _5));

        return true;
    }

    void syncTimeHttpHandler(
        nx_http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx_http::Request request,
        nx_http::Response* const response,
        nx_http::RequestProcessedHandler completionHandler)
    {
        const auto resultCode = QnTimeSyncRestHandler::processRequest(
            request,
            m_timeSynchronizationManager.get(),
            connection->socket().get());
        if (resultCode != nx_http::StatusCode::ok)
            return completionHandler(resultCode);

        // Sending our time synchronization information to remote peer.
        QnTimeSyncRestHandler::prepareResponse(
            *m_timeSynchronizationManager,
            m_commonModule,
            response);

        return completionHandler(nx_http::StatusCode::ok);
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
        ASSERT_TRUE(peer->start());
        m_peers.push_back(std::move(peer));
    }

    void givenMultipleOfflinePeersEachWithDifferentLocalTime()
    {
        const auto peerCount = 3;
        for (int i = 0; i < peerCount; ++i)
        {
            auto peer = std::make_unique<TimeSynchronizationPeer>();
            peer->setSynchronizeWithInternetEnabled(false);
            peer->setOsTimeShift(
                std::chrono::hours(nx::utils::random::number<int>(-1000, 1000)));
            ASSERT_TRUE(peer->start());
            m_peers.push_back(std::move(peer));
        }

        connectEveryPeerToEachOther();
    }

    void givenMultipleSynchronizedPeers()
    {
        givenMultipleOfflinePeersEachWithDifferentLocalTime();
        waitForTimeToBeSynchronizedAcrossAllPeers();
    }

    void shiftLocalTime()
    {
        m_timeShift.applyRelativeShift(-std::chrono::hours(1));
    }

    void restartPeer()
    {
        m_peers.front()->emulateRestart();
    }

    void skewMonotonicClockOnRandomPeer()
    {
        const auto& randomPeer = nx::utils::random::choice(m_peers);
        randomPeer->skewMonotonicClock(std::chrono::seconds(
            nx::utils::random::number<int>(
                kMinMonotonicClockSkew.count(), kMaxMonotonicClockSkew.count())));
    }

    void waitForTimeToBeSynchronizedAcrossAllPeers()
    {
        using namespace std::chrono;

        for (;;)
        {
            const auto t0 = steady_clock::now();
            std::vector<milliseconds> syncTimes;
            for (const auto& peer: m_peers)
                syncTimes.push_back(peer->getSyncTime());
            const auto t1 = steady_clock::now();

            const auto maxDeviation =
                duration_cast<milliseconds>(t1 - t0) +
                milliseconds(*std::max_element(syncTimes.begin(), syncTimes.end()) -
                    *std::min_element(syncTimes.begin(), syncTimes.end()));

            if (maxDeviation <= kMaxAllowedSyncTimeDeviation)
            {
                std::cout << "Time synchronized with " << maxDeviation.count()
                    << "ms deviation" << std::endl;
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
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

    void connectEveryPeerToEachOther()
    {
        for (int i = 0; i < m_peers.size(); ++i)
        {
            for (int j = i+1; j < m_peers.size(); ++j)
                m_peers[i]->connectTo(m_peers[j].get());
        }
    }
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

TEST_F(TimeManager, multiple_peers_synchronize_time)
{
    givenMultipleOfflinePeersEachWithDifferentLocalTime();
    waitForTimeToBeSynchronizedAcrossAllPeers();
}

TEST_F(TimeManager, multiple_peers_synchronize_time_after_monotonic_clock_skew)
{
    givenMultipleSynchronizedPeers();
    skewMonotonicClockOnRandomPeer();
    waitForTimeToBeSynchronizedAcrossAllPeers();
}

// TEST_F(TimeManager, synctime_follows_selected_peer_after_clock_skew_fix)

} // namespace test
} // namespace ec2
