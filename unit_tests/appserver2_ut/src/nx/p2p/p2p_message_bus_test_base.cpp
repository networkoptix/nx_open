#include "p2p_message_bus_test_base.h"

#include <QtCore/QElapsedTimer>

#include <nx/utils/test_support/test_options.h>
#include <nx/utils/log/log.h>
#include <api/common_message_processor.h>
#include <api/global_settings.h>

namespace nx {
namespace p2p {
namespace test {

static const int kMaxInstancesWithTimeManager = 100;

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
        settings->setSynchronizingTimeWithInternet(false);
    }

    settings->synchronizeNow();

    std::vector<ec2::ApiUserData> users;

    for (int i = 0; i < userCount; ++i)
    {
        ec2::ApiUserData userData;
        userData.id = QnUuid::createUuid();
        userData.name = lm("user_%1").arg(i);
        userData.isEnabled = true;
        userData.isCloud = false;
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
    auto userManager = connection->getUserManager(Qn::kSystemAccess);
    auto cameraManager = connection->getCameraManager(Qn::kSystemAccess);
    auto resourceManager = connection->getResourceManager(Qn::kSystemAccess);

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
        auto appserver = createAppserver(i == keepDbAtServerIndex, baseTcpPort);
        appserver->start();
        m_servers.push_back(appserver);
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

} // namespace test
} // namespace p2p
} // namespace nx
