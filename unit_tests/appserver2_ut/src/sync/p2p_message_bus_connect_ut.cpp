#include <gtest/gtest.h>

#include <QtCore/QElapsedTimer>

#include <nx/utils/test_support/module_instance_launcher.h>
#include <test_support/appserver2_process.h>
#include <nx_ec/data/api_camera_data.h>
#include <core/resource_access/user_access_data.h>
#include <nx_ec/ec_api.h>
#include <transaction/transaction_message_bus.h>
#include <nx/p2p/p2p_message_bus.h>
#include <core/resource_management/resource_pool.h>
#include "ec2_thread_pool.h"
#include <nx/network/system_socket.h>
#include "ec2_connection.h"
#include <transaction/transaction_message_bus_base.h>
#include <nx/p2p/p2p_connection.h>
#include <config.h>
#include <nx/p2p/p2p_serialization.h>
#include <nx/utils/test_support/test_options.h>

namespace nx {
namespace p2p {
namespace test {

static const int kInstanceCount = 5;
static const int kMaxSyncTimeoutMs = 1000 * 20 * 1000;
static const int kPropertiesPerCamera = 5;
static const int kCamerasCount = 20;

using Appserver2 = nx::utils::test::ModuleLauncher<::ec2::Appserver2ProcessPublic>;
using Appserver2Ptr = std::unique_ptr<Appserver2>;

class P2pMessageBusTest: public testing::Test
{
protected:

    void initResourceTypes(ec2::AbstractECConnection* ec2Connection)
    {
        QList<QnResourceTypePtr> resourceTypeList;
        const ec2::ErrorCode errorCode = ec2Connection->getResourceManager(Qn::kSystemAccess)->getResourceTypesSync(&resourceTypeList);
        ASSERT_EQ(ec2::ErrorCode::ok, errorCode);
        qnResTypePool->replaceResourceTypeList(resourceTypeList);
    }

    Appserver2Ptr createAppserver()
    {
        static int instanceCounter = 0;

        auto tmpDir = nx::utils::TestOptions::temporaryDirectoryPath();
        if (tmpDir.isEmpty())
            tmpDir = QDir::homePath();
        tmpDir  += lm("/ec2_server_sync_ut.data%1").arg(instanceCounter);
        QDir(tmpDir).removeRecursively();

        Appserver2Ptr result(new Appserver2());

        const QString dbFileArg = lit("--dbFile=%1").arg(tmpDir);
        result->addArg(dbFileArg.toStdString().c_str());

        const QString instanceArg = lit("--moduleInstance=%1").arg(instanceCounter);
        result->addArg(instanceArg.toStdString().c_str());

        ++instanceCounter;

        result->start();
        return result;
    }

    void createData(const Appserver2Ptr& server)
    {
        const auto connection = server->moduleInstance()->ecConnection();

        initResourceTypes(connection);

        {
            ec2::ApiMediaServerData serverData;
            auto resTypePtr = qnResTypePool->getResourceTypeByName("Server");
            ASSERT_TRUE(!resTypePtr.isNull());
            serverData.typeId = resTypePtr->getId();
            serverData.id = server->moduleInstance()->commonModule()->moduleGUID();
            serverData.authKey = QnUuid::createUuid().toString();
            serverData.name = lm("server %1").arg(serverData.id);
            ASSERT_TRUE(!resTypePtr.isNull());
            serverData.typeId = resTypePtr->getId();

            auto serverManager = connection->getMediaServerManager(Qn::kSystemAccess);
            ASSERT_EQ(ec2::ErrorCode::ok, serverManager->saveSync(serverData));
        }

        std::vector<ec2::ApiCameraData> cameras;
        std::vector<ec2::ApiCameraAttributesData> userAttrs;
        ec2::ApiResourceParamWithRefDataList cameraParams;
        cameras.reserve(kCamerasCount);
        const auto& moduleGuid = server->moduleInstance()->commonModule()->moduleGUID();
        auto resTypePtr = qnResTypePool->getResourceTypeByName("Camera");
        ASSERT_TRUE(!resTypePtr.isNull());
        for (int i = 0; i < kCamerasCount; ++i)
        {
            ec2::ApiCameraData cameraData;
            cameraData.typeId = resTypePtr->getId();
            cameraData.parentId = moduleGuid;
            cameraData.vendor = "Invalid camera";
            cameraData.physicalId = QnUuid::createUuid().toString();
            cameraData.name = server->moduleInstance()->endpoint().toString();
            cameraData.id = ec2::ApiCameraData::physicalIdToId(cameraData.physicalId);
            cameras.push_back(std::move(cameraData));

            ec2::ApiCameraAttributesData userAttr;
            userAttr.cameraId = cameraData.id;
            userAttrs.push_back(userAttr);

            for (int j = 0; j < kPropertiesPerCamera; ++j)
                cameraParams.push_back(ec2::ApiResourceParamWithRefData(cameraData.id, lit("property%1").arg(j), lit("value%1").arg(j)));
        }
        auto cameraManager = connection->getCameraManager(Qn::kSystemAccess);
        auto resourceManager = connection->getResourceManager(Qn::kSystemAccess);
        ASSERT_EQ(ec2::ErrorCode::ok, cameraManager->saveUserAttributesSync(userAttrs));
        ASSERT_EQ(ec2::ErrorCode::ok, resourceManager->saveSync(cameraParams));
        ASSERT_EQ(ec2::ErrorCode::ok, cameraManager->addCamerasSync(cameras));
    }

    static bool checkSubscription(const MessageBus* bus, const ApiPersistentIdData& peer)
    {
        return bus->isSubscribedTo(peer);
    }

    static bool checkDistance(const MessageBus* bus, const ApiPersistentIdData& peer)
    {
        return bus->distanceTo(peer) <= kMaxOnlineDistance;
    }

    void checkMessageBus(
        std::function<bool(MessageBus*, const ApiPersistentIdData&)> checkFunction,
        const QString& errorMessage)
    {
        int syncDoneCounter = 0;
        // wait for done
        checkMessageBusInternal(checkFunction, errorMessage, true, syncDoneCounter);
        // report error
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
                const auto& bus = dynamic_cast<MessageBus*>(connection->messageBus());
                const auto& commonModule = server->moduleInstance()->commonModule();
                for (const auto& serverTo: m_servers)
                {
                    const auto& commonModuleTo = serverTo->moduleInstance()->commonModule();
                    ec2::ApiPersistentIdData peer(commonModuleTo->moduleGUID(), commonModuleTo->dbId());
                    bool result = checkFunction(bus, peer);
                    if (!result)
                    {
                        if (!waitForSync)
                            NX_LOG(lit("Peer %1 %2 to peer %3")
                                .arg(qnStaticCommon->moduleDisplayName(commonModule->moduleGUID()))
                                .arg(errorMessage)
                                .arg(qnStaticCommon->moduleDisplayName(commonModuleTo->moduleGUID())),
                                cl_logDEBUG1);
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
        } while (waitForSync && syncDoneCounter != kInstanceCount*kInstanceCount && timer.elapsed() < kMaxSyncTimeoutMs);
    }

    // connect servers: 0 --> 1 -->2 --> N
    static void sequenceConnect(std::vector<Appserver2Ptr>& servers)
    {
        for (int i = 1; i < servers.size(); ++i)
        {
            const auto addr = servers[i]->moduleInstance()->endpoint();
            QUrl url = lit("http://%1:%2/ec2/messageBus").arg(addr.address.toString()).arg(addr.port);
            auto peerId = servers[i]->moduleInstance()->commonModule()->moduleGUID();
            servers[i - 1]->moduleInstance()->ecConnection()->messageBus()->
                addOutgoingConnectionToPeer(peerId, url);
        }
    }

    // connect all servers to each others
    static void fullConnect(std::vector<Appserver2Ptr>& servers)
    {
        for (int i = 0; i < servers.size(); ++i)
        {
            for (int j = 0; j < servers.size(); ++j)
            {
                if (j == i)
                    continue;
                const auto addr = servers[i]->moduleInstance()->endpoint();
                auto peerId = servers[i]->moduleInstance()->commonModule()->moduleGUID();

                QUrl url = lit("http://%1:%2/ec2/messageBus").arg(addr.address.toString()).arg(addr.port);
                servers[j]->moduleInstance()->ecConnection()->messageBus()->
                    addOutgoingConnectionToPeer(peerId, url);
            }
        }
    }

    void printReport()
    {
        ec2::Ec2DirectConnection* connection =
            dynamic_cast<ec2::Ec2DirectConnection*> (m_servers[0]->moduleInstance()->ecConnection());
        ec2::detail::QnDbManager* db = connection->messageBus()->getDb();
        ec2::ApiTransactionDataList tranList;
        db->doQuery(ec2::ApiTranLogFilter(), tranList);
        qint64 totalDbData = 0;
        for (const auto& tran : tranList)
            totalDbData += tran.dataSize;

        NX_LOG(lit("Total bytes sent: %1, dbSize: %2, ratio: %3")
            .arg(nx::network::totalSocketBytesSent())
            .arg(totalDbData)
            .arg(nx::network::totalSocketBytesSent() / (float)totalDbData), cl_logINFO);

        const auto& counters = Connection::sendCounters();
        qint64 webSocketBytes = 0;
        for (int i = 0; i < (int)MessageType::counter; ++i)
            webSocketBytes += counters[i];

        NX_LOG(lit("Total bytes via P2P: %1, dbSize: %2, ratio: %3")
            .arg(webSocketBytes)
            .arg(totalDbData)
            .arg(webSocketBytes / (float)totalDbData), cl_logINFO);

        for (int i = 0; i < (int)MessageType::counter; ++i)
        {
            NX_LOG(lit("P2P message: %1, bytes %2, dbSize: %3, ratio: %4")
                .arg(toString(MessageType(i)))
                .arg(counters[i])
                .arg(totalDbData)
                .arg(counters[i] / (float)totalDbData), cl_logINFO);
        }

        int totalConnections = 0;
        int startedConnections = 0;
        int connectionTries = 0;
        for (const auto& server : m_servers)
        {
            ec2::Ec2DirectConnection* connection =
                dynamic_cast<ec2::Ec2DirectConnection*> (server->moduleInstance()->ecConnection());
            const auto& bus = dynamic_cast<MessageBus*>(connection->messageBus());
            for (const auto& connection : bus->connections())
            {
                ++totalConnections;
                if (bus->context(connection)->isLocalStarted)
                    ++startedConnections;
            }
            connectionTries += bus->connectionTries();
        }

        float k = (kInstanceCount - 1) * kInstanceCount;
        NX_LOG(lit("Total connections: %1, ratio %2.  Opened %3, ratio %4.  Tries: %5, ratio %6")
            .arg(totalConnections)
            .arg(totalConnections / k)
            .arg(startedConnections)
            .arg(startedConnections / k)
            .arg(connectionTries)
            .arg(connectionTries / k),
            cl_logINFO);
    }

    void testMain(std::function<void(std::vector<Appserver2Ptr>&)> serverConnectFunc)
    {
        const_cast<bool&>(ec2::ini().isP2pMode) = true;

        QElapsedTimer t;
        t.restart();

        for (int i = 0; i < kInstanceCount; ++i)
            m_servers.push_back(createAppserver());

        for (const auto& server: m_servers)
        {
            ASSERT_TRUE(server->waitUntilStarted());
            NX_LOG(lit("Server started at url %1").arg(server->moduleInstance()->endpoint().toString()),
                cl_logINFO);
        }

        t.restart();
        for (const auto& server: m_servers)
            createData(server);

        NX_LOG(lit("Create test data time: %1 ms").arg(t.elapsed()), cl_logINFO);


        serverConnectFunc(m_servers);

        QElapsedTimer timer;
        timer.restart();

        // check all peers have subscribed to each other
        checkMessageBus(&checkSubscription, lm("is not subscribed"));

        // check all peers is able to see each other
        checkMessageBus(&checkDistance, lm("has not online distance"));

        // wait for data sync
        int syncDoneCounter = 0;
        do
        {
            syncDoneCounter = 0;
            for (const auto& server: m_servers)
            {
                const auto& resPool = server->moduleInstance()->commonModule()->resourcePool();
                const auto& cameraList = resPool->getAllCameras(QnResourcePtr());
                if (cameraList.size() == kInstanceCount * kCamerasCount)
                    ++syncDoneCounter;
                else
                    break;
            }
            ASSERT_TRUE(timer.elapsed() < kMaxSyncTimeoutMs);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        } while (syncDoneCounter != kInstanceCount && timer.elapsed() < kMaxSyncTimeoutMs);

        NX_LOG(lit("Sync data time: %1 ms").arg(timer.elapsed()), cl_logINFO);
        printReport();
        std::this_thread::sleep_for(std::chrono::seconds(50000));
    }
private:
    QnStaticCommonModule staticCommon;
    std::vector<Appserver2Ptr> m_servers;
};

TEST_F(P2pMessageBusTest, SequenceConnect)
{
    testMain(sequenceConnect);
}

TEST_F(P2pMessageBusTest, FullConnect)
{
    testMain(fullConnect);
}

} // namespace test
} // namespace p2p
} // namespace nx
