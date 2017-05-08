#include <gtest/gtest.h>

#include <QtCore/QElapsedTimer>

#include <nx/utils/test_support/module_instance_launcher.h>
#include <test_support/appserver2_process.h>
#include <nx_ec/data/api_camera_data.h>
#include <core/resource_access/user_access_data.h>
#include <nx_ec/ec_api.h>
#include <transaction/transaction_message_bus.h>
#include <transaction/p2p_message_bus.h>
#include <core/resource_management/resource_pool.h>
#include "ec2_thread_pool.h"
#include <nx/network/system_socket.h>
#include "ec2_connection.h"
#include <transaction/transaction_message_bus_base.h>
#include <transaction/p2p_connection.h>

namespace {

static const int kInstanceCount = 100;
static const int kMaxSyncTimeoutMs = 1000 * 20 * 1000;
static const int kCamerasCount = 100;

static void initResourceTypes(ec2::AbstractECConnection* ec2Connection)
{
    QList<QnResourceTypePtr> resourceTypeList;
    const ec2::ErrorCode errorCode = ec2Connection->getResourceManager(Qn::kSystemAccess)->getResourceTypesSync(&resourceTypeList);
    ASSERT_EQ(ec2::ErrorCode::ok, errorCode);
    qnResTypePool->replaceResourceTypeList(resourceTypeList);
}

using Appserver2 = nx::utils::test::ModuleLauncher<::ec2::Appserver2ProcessPublic>;
using Appserver2Ptr = std::unique_ptr<Appserver2>;

static Appserver2Ptr createAppserver()
{
    static int instanceCounter = 0;

    const auto tmpDir =
        (QDir::homePath() + "/ec2_server_sync_ut.data") + QString::number(instanceCounter);
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

static void createData(const Appserver2Ptr& server)
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
    for (int i = 0; i < kCamerasCount; ++i)
    {
        ec2::ApiCameraData cameraData;
        auto resTypePtr = qnResTypePool->getResourceTypeByName("Camera");
        ASSERT_TRUE(!resTypePtr.isNull());
        cameraData.typeId = resTypePtr->getId();
        cameraData.parentId = server->moduleInstance()->commonModule()->moduleGUID();
        cameraData.vendor = "Invalid camera";
        cameraData.physicalId = QnUuid::createUuid().toString();
        cameraData.name = server->moduleInstance()->endpoint().toString();
        cameraData.id = ec2::ApiCameraData::physicalIdToId(cameraData.physicalId);

        auto cameraManager = connection->getCameraManager(Qn::kSystemAccess);
        ASSERT_EQ(ec2::ErrorCode::ok, cameraManager->addCameraSync(cameraData));
    }
}

} // namespace

void checkDistance(const std::vector<Appserver2Ptr>& servers, bool waitForSync, int& syncDoneCounter)
{
    QElapsedTimer timer;
    timer.restart();
    do
    {
        syncDoneCounter = 0;
        for (const auto& server : servers)
        {
            const auto& connection = server->moduleInstance()->ecConnection();
            const auto& bus = connection->p2pMessageBus();
            const auto& commonModule = server->moduleInstance()->commonModule();
            for (const auto& serverTo : servers)
            {
                const auto& commonModuleTo = serverTo->moduleInstance()->commonModule();
                ec2::ApiPersistentIdData peer(commonModuleTo->moduleGUID(), commonModuleTo->dbId());
                if (bus->distanceTo(peer) > ec2::kMaxOnlineDistance)
                {
                    if (!waitForSync)
                        NX_LOG(lit("Peer %1 has not online distance to peer %2")
                            .arg(qnStaticCommon->moduleDisplayName(commonModule->moduleGUID()))
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

void checkSubscription(const std::vector<Appserver2Ptr>& servers, bool waitForSync, int& syncDoneCounter)
{
    QElapsedTimer timer;
    timer.restart();
    do
    {
        syncDoneCounter = 0;
        for (const auto& server: servers)
        {
            const auto& connection = server->moduleInstance()->ecConnection();
            const auto& bus = connection->p2pMessageBus();
            const auto& commonModule = server->moduleInstance()->commonModule();
            for (const auto& serverTo : servers)
            {
                const auto& commonModuleTo = serverTo->moduleInstance()->commonModule();
                ec2::ApiPersistentIdData peer(commonModuleTo->moduleGUID(), commonModuleTo->dbId());
                if (!bus->isSubscribedTo(peer))
                {
                    if (!waitForSync)
                        NX_LOG(lit("Peer %1 is not subscribed to peer %2")
                            .arg(qnStaticCommon->moduleDisplayName(commonModule->moduleGUID()))
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
void sequenceConnect(std::vector<Appserver2Ptr>& servers)
{
    for (int i = 1; i < servers.size(); ++i)
    {
        const auto addr = servers[i]->moduleInstance()->endpoint();
        QUrl url = lit("http://%1:%2/ec2/messageBus").arg(addr.address.toString()).arg(addr.port);
        ec2::ApiPersistentIdData peer(
            servers[i]->moduleInstance()->commonModule()->moduleGUID(),
            servers[i]->moduleInstance()->commonModule()->dbId()
            );
        servers[i - 1]->moduleInstance()->ecConnection()->p2pMessageBus()->
            addOutgoingConnectionToPeer(peer, url);
    }
}

// connect all servers to each others
void fullConnect(std::vector<Appserver2Ptr>& servers)
{
    for (int i = 0; i < servers.size(); ++i)
    {
        for (int j = 0; j < servers.size(); ++j)
        {
            if (j == i)
                continue;
            const auto addr = servers[i]->moduleInstance()->endpoint();
            ec2::ApiPersistentIdData peer(
                servers[i]->moduleInstance()->commonModule()->moduleGUID(),
                servers[i]->moduleInstance()->commonModule()->dbId()
            );

            QUrl url = lit("http://%1:%2/ec2/messageBus").arg(addr.address.toString()).arg(addr.port);
            servers[j]->moduleInstance()->ecConnection()->p2pMessageBus()->
                addOutgoingConnectionToPeer(peer, url);
        }
    }
}

static void testMain(std::function<void (std::vector<Appserver2Ptr>&)> serverConnectFunc)
{
    QElapsedTimer t;
    t.restart();

    QnStaticCommonModule staticCommon;

    std::vector<Appserver2Ptr> servers;
    for (int i = 0; i < kInstanceCount; ++i)
        servers.push_back(createAppserver());

    for (const auto& server : servers)
    {
        ASSERT_TRUE(server->waitUntilStarted());
        NX_LOG(lit("Server started at url %1").arg(server->moduleInstance()->endpoint().toString()),
            cl_logINFO);
    }

    //std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    t.restart();
    for (const auto& server: servers)
        createData(server);

    NX_LOG(lit("Create test data time: %1 ms").arg(t.elapsed()), cl_logINFO);


    serverConnectFunc(servers);

    int syncDoneCounter = 0;
    QElapsedTimer timer;
    timer.restart();

    // check subscription
    checkSubscription(servers, true, syncDoneCounter);
    if (syncDoneCounter != servers.size() * servers.size())
        checkSubscription(servers, false, syncDoneCounter); //< report detailed error

    // check peers distances
    checkDistance(servers, true, syncDoneCounter);
    if (syncDoneCounter != servers.size() * servers.size())
        checkDistance(servers, false, syncDoneCounter); //< report detailed error

    // wait for data sync
    do
    {
        syncDoneCounter = 0;
        for (const auto& server : servers)
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


    ec2::Ec2DirectConnection* connection =
        dynamic_cast<ec2::Ec2DirectConnection*> (servers[0]->moduleInstance()->ecConnection());
    ec2::detail::QnDbManager* db = connection->p2pMessageBus()->getDb();
    ec2::ApiTransactionDataList tranList;
    db->doQuery(ec2::ApiTranLogFilter(), tranList);
    qint64 totalDbData = 0;
    for (const auto& tran: tranList)
        totalDbData += tran.dataSize;


    NX_LOG(lit("Sync data time: %1 ms").arg(timer.elapsed()), cl_logINFO);
    NX_LOG(lit("Total bytes sent: %1, dbSize: %2, ratio: %3")
        .arg(nx::network::totalSocketBytesSent())
        .arg(totalDbData)
        .arg(nx::network::totalSocketBytesSent() / (float) totalDbData), cl_logINFO);

    const auto& counters = ec2::P2pConnection::sendCounters();
    qint64 webSocketBytes = 0;
    for (int i = 0; i < (int)ec2::MessageType::counter; ++i)
        webSocketBytes += counters[i];

    NX_LOG(lit("Total bytes via P2P: %1, dbSize: %2, ratio: %3")
        .arg(webSocketBytes)
        .arg(totalDbData)
        .arg(webSocketBytes / (float)totalDbData), cl_logINFO);

    for (int i = 0; i < (int)ec2::MessageType::counter; ++i)
    {
        NX_LOG(lit("P2P message: %1, bytes %2, dbSize: %3, ratio: %4")
            .arg(toString(ec2::MessageType(i)))
            .arg(counters[i])
            .arg(totalDbData)
            .arg(counters[i] / (float)totalDbData), cl_logINFO);
    }

    int totalConnections = 0;
    int startedConnections = 0;
    int connectionTries = 0;
    for (const auto& server: servers)
    {
        ec2::Ec2DirectConnection* connection =
            dynamic_cast<ec2::Ec2DirectConnection*> (server->moduleInstance()->ecConnection());
        const auto bus = connection->p2pMessageBus();
        for (const auto& connection : bus->connections())
        {
            ++totalConnections;
            if (connection->miscData().isLocalStarted)
                ++startedConnections;
        }
        connectionTries += bus->connectionTries();
    }

    float k = (kInstanceCount-1) * kInstanceCount;
    NX_LOG(lit("Total connections: %1, ratio %2.  Opened %3, ratio %4.  Tries: %5, ratio %6")
        .arg(totalConnections)
        .arg(totalConnections / k)
        .arg(startedConnections)
        .arg(startedConnections / k)
        .arg(connectionTries)
        .arg(connectionTries / k),
        cl_logINFO);


    std::this_thread::sleep_for(std::chrono::seconds(5000));
}

TEST(P2pMessageBus, SequenceConnect)
{
    testMain(sequenceConnect);
}

TEST(P2pMessageBus, FullConnect)
{
    testMain(fullConnect);
}

TEST(P2pMessageBus, CompressPeerNumber)
{
    using namespace ec2;

    QVector<PeerNumberType> peers;
    for (int i = 0; i < kMaxOnlineDistance; ++i)
        peers.push_back(i);
    QVector<PeerNumberType> peers2;

    try
    {

        // serialize
        QByteArray serializedData;
        serializedData.resize(peers.size() * 2);
        BitStreamWriter writer;
        writer.setBuffer((quint8*) serializedData.data(), serializedData.size());
        for (const auto& peer : peers)
            serializeCompressPeerNumber(writer, peer);
        writer.flushBits();
        serializedData.truncate(writer.getBytesCount());

        // deserialize back
        BitStreamReader reader((const quint8*)serializedData.data(), serializedData.size());
        while (reader.bitsLeft() >= 8)
            peers2.push_back(deserializeCompressPeerNumber(reader));
    }
    catch (...)
    {
        ASSERT_TRUE(0);
    }
    ASSERT_EQ(peers, peers2);
}

TEST(P2pMessageBus, CompressPeerNumber2)
{
    using namespace ec2;

    try
    {
        auto writeData = [](QByteArray& buffer, int filler)
        {
            buffer.resize(4);
            buffer[0] = buffer[1] = filler;
            BitStreamWriter writer;
            writer.setBuffer((quint8*)buffer.data(), buffer.size());
            serializeCompressPeerNumber(writer, 1023);
            writer.flushBits(true);
            buffer.truncate(2);
        };

        // serialize
        QByteArray data1;
        QByteArray data2;
        writeData(data1, 0);
        writeData(data2, 0xff);
        ASSERT_EQ(data1, data2);
    }
    catch (...)
    {
        ASSERT_TRUE(0);
    }
}

TEST(P2pMessageBus, SerializePeers)
{
    using namespace ec2;
    ApiPersistentIdData localPeer;
    localPeer.id = QnUuid::createUuid();
    localPeer.persistentId = QnUuid::createUuid();
    P2pMessageBus::BidirectionRoutingInfo peerData(localPeer);
    PeerNumberInfo shortPeers;

    std::vector<ApiPersistentIdData> peers;
    for (int i = 0; i < 2; ++i)
    {
        ApiPersistentIdData p;
        p.id = QnUuid::createUuid();
        p.persistentId = QnUuid::createUuid();
        peers.push_back(p);
        shortPeers.encode(p);
        peerData.addRecord(localPeer, p, P2pMessageBus::RoutingRecord(1, 0));
    }

    QByteArray serializedData = P2pMessageBus::serializePeersMessage(&peerData, shortPeers);

    P2pMessageBus::BidirectionRoutingInfo deserializedPeerData(localPeer);
    ASSERT_TRUE(P2pMessageBus::deserializePeersMessage(
        localPeer, 
        0, //< distance
        shortPeers, 
        serializedData.mid(1), //< skip message type
        /*time*/0,
        &deserializedPeerData));
    QByteArray serializedData2 = P2pMessageBus::serializePeersMessage(&deserializedPeerData, shortPeers);
    ASSERT_EQ(serializedData, serializedData2);
    using namespace ec2;

}
