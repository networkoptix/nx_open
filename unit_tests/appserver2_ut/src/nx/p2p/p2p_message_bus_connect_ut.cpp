#include "p2p_message_bus_test_base.h"

#include <QtCore/QElapsedTimer>

#include <nx_ec/data/api_camera_data.h>
#include <core/resource_access/user_access_data.h>

#include <nx/p2p/p2p_message_bus.h>
#include <core/resource_management/resource_pool.h>
#include "ec2_thread_pool.h"
#include <nx/network/system_socket.h>
#include "ec2_connection.h"
#include <transaction/transaction_message_bus_base.h>
#include <nx/p2p/p2p_connection.h>
#include <nx/p2p/p2p_serialization.h>
#include <nx_ec/dummy_handler.h>
#include <nx/utils/argument_parser.h>
#include <core/resource/media_server_resource.h>

namespace nx {
namespace p2p {
namespace test {

static const int kDefaultInstanceCount = 3;
static const int kDefaultPropertiesPerCamera = 5;
static const int kDefaultCamerasCount = 20;
static const int kDefaultUsersCount = 0;

static const char kServerCountParamName[] = "serverCount";
static const char kCameraCountParamName[] = "cameraCount";
static const char kPropertyCountParamName[] = "propertyCount";
static const char kUserCountParamName[] = "userCount";
static const char kServerPortParamName[] = "tcpPort";
static const char kStandaloneModeParamName[] = "standaloneMode";

static const int kMaxSyncTimeoutMs = 1000 * 20 * 1000;

int getIntParam(const nx::utils::ArgumentParser& args, const QString& name, int defaultValue = 0)
{
    auto result = args.get(name);
    return result ? result->toInt() : defaultValue;
}

class P2pMessageBusTest: public P2pMessageBusTestBase
{
protected:

    static bool checkSubscription(const MessageBus* bus, const ApiPersistentIdData& peer)
    {
        return bus->isSubscribedTo(peer);
    }

    static bool checkDistance(const MessageBus* bus, const ApiPersistentIdData& peer)
    {
        return bus->distanceTo(peer) <= kMaxOnlineDistance;
    }

    bool checkRuntimeInfo(const MessageBus* bus, const ApiPersistentIdData& /*peer*/)
    {
        return bus->runtimeInfo().size() == m_servers.size();
    }

    void checkMessageBus(
        std::function<bool(MessageBus*, const ApiPersistentIdData&)> checkFunction,
        const QString& errorMessage)
    {
        int syncDoneCounter = 0;
        // Wait until done
        checkMessageBusInternal(checkFunction, errorMessage, true, syncDoneCounter);
        // Report error
        if (syncDoneCounter != m_servers.size() * m_servers.size())
            checkMessageBusInternal(checkFunction, errorMessage, false, syncDoneCounter);
    }

    void checkMessageBusInternal(
        std::function<bool (MessageBus*, const ApiPersistentIdData&)> checkFunction,
        const QString& errorMessage,
        bool waitForSync,
        int& syncDoneCounter)
    {
        QElapsedTimer timer;
        timer.restart();
        do
        {
            syncDoneCounter = 0;
            for (const auto& server: m_servers)
            {
                const auto& connection = server->moduleInstance()->ecConnection();
                const auto& bus = connection->messageBus()->dynamicCast<MessageBus*>();
                const auto& commonModule = server->moduleInstance()->commonModule();
                for (const auto& serverTo: m_servers)
                {
                    const auto& commonModuleTo = serverTo->moduleInstance()->commonModule();
                    ec2::ApiPersistentIdData peer(commonModuleTo->moduleGUID(), commonModuleTo->dbId());
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
        } while (waitForSync && syncDoneCounter != m_servers.size()*m_servers.size() && timer.elapsed() < kMaxSyncTimeoutMs);
    }

    void printReport()
    {
        ec2::Ec2DirectConnection* connection =
            dynamic_cast<ec2::Ec2DirectConnection*> (m_servers[0]->moduleInstance()->ecConnection());
        ec2::detail::QnDbManager* db = connection->messageBus()->getDb();
        ec2::ApiTransactionDataList tranList;
        db->doQuery(ec2::ApiTranLogFilter(), tranList);
        qint64 totalDbData = 0;
        for (const auto& tran: tranList)
            totalDbData += tran.dataSize;

        NX_INFO(this, lit("Total bytes sent: %1, dbSize: %2, ratio: %3")
            .arg(nx::network::totalSocketBytesSent())
            .arg(totalDbData)
            .arg(nx::network::totalSocketBytesSent() / (float)totalDbData));

        const auto& counters = Connection::sendCounters();
        qint64 webSocketBytes = 0;
        for (int i = 0; i < (int) MessageType::counter; ++i)
            webSocketBytes += counters[i];

        NX_INFO(this, lit("Total bytes via P2P: %1, dbSize: %2, ratio: %3")
            .arg(webSocketBytes)
            .arg(totalDbData)
            .arg(webSocketBytes / (float)totalDbData));

        for (int i = 0; i < (int) MessageType::counter; ++i)
        {
            NX_INFO(this, lit("P2P message: %1, bytes %2, dbSize: %3, ratio: %4")
                .arg(toString(MessageType(i)))
                .arg(counters[i])
                .arg(totalDbData)
                .arg(counters[i] / (float)totalDbData));
        }

        int totalConnections = 0;
        int startedConnections = 0;
        int connectionTries = 0;
        for (const auto& server: m_servers)
        {
            ec2::Ec2DirectConnection* connection =
                dynamic_cast<ec2::Ec2DirectConnection*> (server->moduleInstance()->ecConnection());
            const auto& bus = connection->messageBus()->dynamicCast<MessageBus*>();
            for (const auto& connection: bus->connections())
            {
                ++totalConnections;
                if (bus->context(connection)->isLocalStarted)
                    ++startedConnections;
            }
            connectionTries += bus->connectionTries();
        }

        float k = (m_servers.size() - 1) * m_servers.size();
        NX_INFO(
            this,
            lit("Total connections: %1, ratio %2.  Opened %3, ratio %4.  Tries: %5, ratio %6")
            .arg(totalConnections)
            .arg(totalConnections / k)
            .arg(startedConnections)
            .arg(startedConnections / k)
            .arg(connectionTries)
            .arg(connectionTries / k));
    }

    void addRuntimeData(const Appserver2Ptr& server)
    {
        auto commonModule = server->moduleInstance()->commonModule();
        auto connection = server->moduleInstance()->ecConnection();
        ApiRuntimeData runtimeData;
        runtimeData.peer.id = commonModule->moduleGUID();
        runtimeData.peer.instanceId = commonModule->runningInstanceGUID();
        runtimeData.peer.persistentId = commonModule->dbId();
        runtimeData.peer.peerType = Qn::PT_Server;

        connection->getMiscManager(Qn::kSystemAccess)
            ->saveRuntimeInfo(
                runtimeData,
                ec2::DummyHandler::instance(),
                &ec2::DummyHandler::onRequestDone);
    }

    void testMain(std::function<void(std::vector<Appserver2Ptr>&)> serverConnectFunc, int keepDbAtServerIndex = -1)
    {
        nx::utils::ArgumentParser args(QCoreApplication::instance()->arguments());

        m_servers.clear();
        m_instanceCounter = 0;

        const int instanceCount = getIntParam(args, kServerCountParamName, kDefaultInstanceCount);
        const int serverPort = getIntParam(args, kServerPortParamName);
        startServers(instanceCount, keepDbAtServerIndex, serverPort);

        QElapsedTimer t;
        t.restart();
        const int cameraCount = getIntParam(args, kCameraCountParamName, kDefaultCamerasCount);
        for (const auto& server: m_servers)
        {
            createData(
                server,
                cameraCount,
                getIntParam(args, kPropertyCountParamName, kDefaultPropertiesPerCamera),
                getIntParam(args, kUserCountParamName, kDefaultUsersCount));
            addRuntimeData(server);
        }
        NX_LOG(lit("Create test data time: %1 ms").arg(t.elapsed()), cl_logINFO);

        serverConnectFunc(m_servers);

        QElapsedTimer timer;
        timer.restart();

        // Check all peers have subscribed to each other
        checkMessageBus(&checkSubscription, lm("is not subscribed"));

        // Check all peers is able to see each other
        checkMessageBus(&checkDistance, lm("has not online distance"));

        // Check all runtime data are received
        using namespace std::placeholders;
        checkMessageBus(std::bind(&P2pMessageBusTest::checkRuntimeInfo, this, _1, _2), lm("missing runtime info"));

        int expectedCamerasCount = instanceCount;
        expectedCamerasCount *= cameraCount;
        if (keepDbAtServerIndex >= 0)
            expectedCamerasCount *= 2;
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
        } while (syncDoneCounter != instanceCount && timer.elapsed() < kMaxSyncTimeoutMs);

        NX_LOG(lit("Sync data time: %1 ms").arg(timer.elapsed()), cl_logINFO);
        printReport();

        for (auto& server: m_servers)
        {
            auto commonModule = server->moduleInstance()->commonModule();
            auto serverRes = commonModule->resourcePool()->
                getResourceById<QnMediaServerResource>(commonModule->moduleGUID());
            auto flags = serverRes->getServerFlags();
            flags |= Qn::SF_P2pSyncDone;
            serverRes->setServerFlags(flags);
            commonModule->bindModuleinformation(serverRes);
        }

        while (args.get(kStandaloneModeParamName))
            std::this_thread::sleep_for(std::chrono::seconds(1));
    }
};

TEST_F(P2pMessageBusTest, SequenceConnect)
{
    testMain(sequenceConnect);
}

TEST_F(P2pMessageBusTest, FullConnect)
{
    testMain(fullConnect);
}

TEST_F(P2pMessageBusTest, RestartServer)
{
    testMain(fullConnect);
    testMain(fullConnect, /*keepDbAtServerIndex*/ 0);
}

} // namespace test
} // namespace p2p
} // namespace nx
