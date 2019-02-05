#include "p2p_message_bus_test_base.h"

#include <QtCore/QElapsedTimer>

#include <api/common_message_processor.h>
#include <api/global_settings.h>

#include <nx/network/app_info.h>
#include <nx/utils/test_support/test_options.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/data/peer_data.h>

namespace nx {
namespace p2p {
namespace test {

static const int kMaxInstancesWithTimeManager = 100;
static const int kMaxSyncTimeoutMs = 1000 * 30;

static const QString kP2pTestSystemName(lit("P2pTestSystem"));

void P2pMessageBusTestBase::createData(
    const Appserver2Ptr& server,
    int camerasCount,
    int propertiesPerCamera,
    int userCount)
{
    ASSERT_TRUE(server->moduleInstance()->createInitialData(kP2pTestSystemName));

    const auto settings = server->moduleInstance()->commonModule()->globalSettings();
    const bool disableTimeManager = m_servers.size() > kMaxInstancesWithTimeManager;
    if (disableTimeManager)
    {
        settings->setTimeSynchronizationEnabled(false);
    }

    settings->synchronizeNow();

    std::vector<nx::vms::api::UserData> users;

    for (int i = 0; i < userCount; ++i)
    {
        nx::vms::api::UserData userData;
        userData.id = QnUuid::createUuid();
        userData.name = lm("user_%1").arg(i);
        userData.isEnabled = true;
        userData.isCloud = false;
        userData.realm = nx::network::AppInfo::realm();
        users.push_back(userData);
    }

    std::vector<nx::vms::api::CameraData> cameras;
    std::vector<nx::vms::api::CameraAttributesData> userAttrs;
    nx::vms::api::ResourceParamWithRefDataList cameraParams;
    cameras.reserve(camerasCount);
    const auto& moduleGuid = server->moduleInstance()->commonModule()->moduleGUID();
    auto resTypePtr = qnResTypePool->getResourceTypeByName("Camera");
    ASSERT_TRUE(!resTypePtr.isNull());
    for (int i = 0; i < camerasCount; ++i)
    {
        nx::vms::api::CameraData cameraData;
        cameraData.typeId = resTypePtr->getId();
        cameraData.parentId = moduleGuid;
        cameraData.vendor = "Invalid camera";
        cameraData.physicalId = QnUuid::createUuid().toString();
        cameraData.name = server->moduleInstance()->endpoint().toString();
        cameraData.id = nx::vms::api::CameraData::physicalIdToId(cameraData.physicalId);
        cameras.push_back(std::move(cameraData));

        nx::vms::api::CameraAttributesData userAttr;
        userAttr.cameraId = cameraData.id;
        userAttrs.push_back(userAttr);

        for (int j = 0; j < propertiesPerCamera; ++j)
        {
            cameraParams.push_back(
                nx::vms::api::ResourceParamWithRefData(
                    cameraData.id,
                    lit("property%1").arg(j),
                    lit("value%1").arg(j)));
        }
    }
    auto connection = server->moduleInstance()->commonModule()->ec2Connection();
    auto userManager = connection->makeUserManager(Qn::kSystemAccess);
    auto cameraManager = connection->makeCameraManager(Qn::kSystemAccess);
    auto resourceManager = connection->makeResourceManager(Qn::kSystemAccess);

    for (const auto& user: users)
        ASSERT_EQ(ec2::ErrorCode::ok, userManager->saveSync(user));
    ASSERT_EQ(ec2::ErrorCode::ok, cameraManager->saveUserAttributesSync(userAttrs));
    ASSERT_EQ(ec2::ErrorCode::ok, resourceManager->saveSync(cameraParams));
    ASSERT_EQ(ec2::ErrorCode::ok, cameraManager->addCamerasSync(cameras));
}

void P2pMessageBusTestBase::startServers(int count, int keepDbAtServerIndex, quint16 baseTcpPort)
{
    QElapsedTimer t;
    t.restart();
    for (int i = 0; i < count; ++i)
    {
        auto appserver = Appserver2Launcher::createAppserver(i == keepDbAtServerIndex, baseTcpPort);
        appserver->start();
        m_servers.push_back(std::move(appserver));
    }

    for (const auto& server: m_servers)
    {
        ASSERT_TRUE(server->waitUntilStarted());
        auto commonModule = server->moduleInstance()->commonModule();
        NX_INFO(this, lit("Server %1 started at url %2")
            .arg(qnStaticCommon->moduleDisplayName(commonModule->moduleGUID()))
            .arg(server->moduleInstance()->endpoint().toString()));
    }
}

void P2pMessageBusTestBase::bidirectConnectServers(const Appserver2Ptr& srcServer, const Appserver2Ptr& dstServer)
{
    connectServers(srcServer, dstServer);
    connectServers(dstServer, srcServer);
}

void P2pMessageBusTestBase::connectServers(const Appserver2Ptr& srcServer, const Appserver2Ptr& dstServer)
{
    srcServer->moduleInstance()->connectTo(dstServer->moduleInstance().get());
}

void P2pMessageBusTestBase::disconnectServers(const Appserver2Ptr& srcServer, const Appserver2Ptr& dstServer)
{
    const auto addr = dstServer->moduleInstance()->endpoint();
    auto peerId = dstServer->moduleInstance()->commonModule()->moduleGUID();

    srcServer->moduleInstance()->ecConnection()->messageBus()->
        removeOutgoingConnectionFromPeer(peerId);
}

// connect servers: 0 --> 1 -->2 --> N
void P2pMessageBusTestBase::sequenceConnect(std::vector<Appserver2Ptr>& servers)
{
    for (int i = 1; i < servers.size(); ++i)
        connectServers(servers[i - 1], servers[i]);
}

// connect servers: 0 --> 1 -->2 --> N --> 0
void P2pMessageBusTestBase::circleConnect(std::vector<Appserver2Ptr>& servers)
{
    sequenceConnect(servers);
    connectServers(servers[servers.size()-1], servers[0]);
}

// connect all servers to each others
void P2pMessageBusTestBase::fullConnect(std::vector<Appserver2Ptr>& servers)
{
    for (int i = 0; i < servers.size(); ++i)
    {
        for (int j = 0; j < servers.size(); ++j)
        {
            if (j == i)
                continue;
            connectServers(servers[j], servers[i]);
        }
    }
}

// connect all servers to each others
void P2pMessageBusTestBase::emptyConnect(std::vector<Appserver2Ptr>& servers)
{
}

bool P2pMessageBusTestBase::waitForCondition(
    std::function<bool()> condition,
    std::chrono::milliseconds timeout)
{
    QElapsedTimer timer;
    timer.restart();
    while (!condition() && timer.elapsed() < timeout.count())
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return condition();
}

bool P2pMessageBusTestBase::waitForConditionOnAllServers(
    std::function<bool(const Appserver2Ptr&)> condition,
    std::chrono::milliseconds timeout)
{
    int syncDoneCounter = 0;
    QElapsedTimer timer;
    timer.restart();
    do
    {
        syncDoneCounter = 0;
        for (const auto& server: m_servers)
        {
            if (condition(server))
                ++syncDoneCounter;
            else
                break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    } while (syncDoneCounter != m_servers.size() && timer.elapsed() < timeout.count());
    return timer.elapsed() < timeout.count();
}

void P2pMessageBusTestBase::checkMessageBus(
    std::function<bool(MessageBus*, const vms::api::PersistentIdData&)> checkFunction,
    const QString& errorMessage)
{
    int syncDoneCounter = 0;
    // Wait until done
    checkMessageBusInternal(checkFunction, errorMessage, /*waitForSync*/ true, &syncDoneCounter);
    // Report error
    if (syncDoneCounter != m_servers.size() * m_servers.size())
    {
        checkMessageBusInternal(
            checkFunction, errorMessage, /*waitForSync*/ false, &syncDoneCounter);
    }
}

void P2pMessageBusTestBase::checkMessageBusInternal(
    std::function<bool(MessageBus*, const vms::api::PersistentIdData&)> checkFunction,
    const QString& errorMessage,
    bool waitForSync,
    int* outSyncDoneCounter)
{
    int& syncDoneCounter = *outSyncDoneCounter;

    QElapsedTimer timer;
    timer.restart();
    do
    {
        *outSyncDoneCounter = 0;
        for (const auto& server : m_servers)
        {
            const auto& connection = server->moduleInstance()->ecConnection();
            const auto& bus = connection->messageBus()->dynamicCast<MessageBus*>();
            const auto& commonModule = server->moduleInstance()->commonModule();
            for (const auto& serverTo : m_servers)
            {
                const auto& commonModuleTo = serverTo->moduleInstance()->commonModule();
                vms::api::PersistentIdData peer(commonModuleTo->moduleGUID(), commonModuleTo->dbId());
                bool result = checkFunction(bus, peer);
                if (!result)
                {
                    if (!waitForSync)
                        NX_DEBUG(
                            this,
                            lit("Peer %1 %2 to peer %3")
                            .arg(qnStaticCommon->moduleDisplayName(commonModule->moduleGUID()))
                            .arg(errorMessage)
                            .arg(qnStaticCommon->moduleDisplayName(commonModuleTo->moduleGUID())));
                }
                else
                {
                    ++syncDoneCounter;
                }
                if (!waitForSync)
                    ASSERT_TRUE(bus->isSubscribedTo(peer));
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    } while (waitForSync && syncDoneCounter != m_servers.size()*m_servers.size()
        && timer.elapsed() < kMaxSyncTimeoutMs);
}

bool P2pMessageBusTestBase::checkSubscription(
    const MessageBus* bus, const vms::api::PersistentIdData& peer)
{
    return bus->isSubscribedTo(peer);
}

bool P2pMessageBusTestBase::checkDistance(const MessageBus* bus,
    const vms::api::PersistentIdData& peer)
{
    return bus->distanceTo(peer) <= kMaxOnlineDistance;
}

bool P2pMessageBusTestBase::checkRuntimeInfo(const MessageBus* bus,
    const vms::api::PersistentIdData& /*peer*/)
{
    return bus->runtimeInfo().size() == m_servers.size();
}

void P2pMessageBusTestBase::waitForSync(int cameraCount)
{
    QElapsedTimer timer;
    timer.restart();

    // Check all peers have subscribed to each other
    checkMessageBus(&checkSubscription, lm("is not subscribed"));

    // Check all peers is able to see each other
    checkMessageBus(&checkDistance, lm("has not online distance"));

    // Check all runtime data are received
    using namespace std::placeholders;
    checkMessageBus(std::bind(&P2pMessageBusTestBase::checkRuntimeInfo, this, _1, _2), lm("missing runtime info"));

    int expectedCamerasCount = (int) m_servers.size();
    expectedCamerasCount *= cameraCount;
    // wait for data sync
    int syncDoneCounter = 0;
    do
    {
        syncDoneCounter = 0;
        for (const auto& server: m_servers)
        {
            const auto& resPool = server->moduleInstance()->commonModule()->resourcePool();
            const auto& cameraList = resPool->getAllCameras(QnResourcePtr());
            if (cameraList.size() == expectedCamerasCount)
                ++syncDoneCounter;
            else
                break;
        }
        ASSERT_TRUE(timer.elapsed() < kMaxSyncTimeoutMs);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    } while (syncDoneCounter != m_servers.size() && timer.elapsed() < kMaxSyncTimeoutMs);
    NX_INFO(this, lit("Sync data time: %1 ms").arg(timer.elapsed()));
}

void P2pMessageBusTestBase::setLowDelayIntervals()
{
    for (const auto& server: m_servers)
    {
        auto commonModule = server->moduleInstance()->commonModule();
        const auto connection = server->moduleInstance()->ecConnection();
        MessageBus* bus = connection->messageBus()->dynamicCast<MessageBus*>();
        auto intervals = bus->delayIntervals();
        intervals.sendPeersInfoInterval = std::chrono::milliseconds(1);
        intervals.outConnectionsInterval = std::chrono::milliseconds(1);
        intervals.subscribeIntervalLow = std::chrono::milliseconds(1);
        bus->setDelayIntervals(intervals);
    }
}

} // namespace test
} // namespace p2p
} // namespace nx
